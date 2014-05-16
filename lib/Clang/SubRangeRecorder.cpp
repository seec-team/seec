//===- lib/Clang/SubRangeRecorder.cpp -------------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/SubRangeRecorder.hpp"
#include "seec/Util/MakeUnique.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

/// \brief Records the range of each Stmt in a pretty-printed Stmt.
///
class SubRangeRecorder : public clang::PrinterHelper
{
  clang::PrintingPolicy &Policy;
  
  std::string Buffer;
  
  llvm::raw_string_ostream BufferOS;
  
  llvm::DenseMap<clang::Stmt *, PrintedStmtRange> Ranges;
  
public:
  SubRangeRecorder(clang::PrintingPolicy &WithPolicy)
  : Policy(WithPolicy),
    Buffer(),
    BufferOS(Buffer),
    Ranges()
  {}
  
  virtual bool handledStmt(clang::Stmt *E, llvm::raw_ostream &OS) {
    // Print the Stmt to determine the length of its printed text.
    Buffer.clear();
    E->printPretty(BufferOS, nullptr, Policy);
    BufferOS.flush();
    
    // Record the start and length of the Stmt's printed text.
    Ranges.insert(std::make_pair(E,
                                 PrintedStmtRange(OS.tell(), Buffer.size())));
    
    return false;
  }
  
  llvm::DenseMap<clang::Stmt *, PrintedStmtRange> moveRanges() {
    return std::move(Ranges);
  }
};

llvm::DenseMap<clang::Stmt *, PrintedStmtRange>
printStmtAndRecordRanges(llvm::raw_ostream &OS,
                         clang::Stmt const *E,
                         clang::PrintingPolicy &Policy)
{
  SubRangeRecorder PrinterHelper(Policy);
  E->printPretty(OS, &PrinterHelper, Policy);
  OS.flush();
  return PrinterHelper.moveRanges();
}

//===----------------------------------------------------------------------===//
// FormattedStmt
//===----------------------------------------------------------------------===//

FormattedStmtRange const *
FormattedStmt::getStmtRange(clang::Stmt const * const S) const
{
  auto const It = StmtRanges.find(S);
  return It != StmtRanges.end() ? &(It->second) : nullptr;
}

//===----------------------------------------------------------------------===//
// Implementation of formatStmtSource() follows. This is used to get
// an expression's source code with user macros expanded, and record
// sub-expression ranges.
//===----------------------------------------------------------------------===//

/// Adapted from clang/lib/Rewrite/Frontend/RewriteMacros.cpp:
/// isSameToken - Return true if the two specified tokens start have the same
/// content.
static bool isSameToken(clang::Token &RawTok, clang::Token &PPTok) {
  // If two tokens have the same kind and the same identifier info, they are
  // obviously the same.
  if (PPTok.getKind() == RawTok.getKind() &&
      PPTok.getIdentifierInfo() == RawTok.getIdentifierInfo())
    return true;

  // Otherwise, if they are different but have the same identifier info, they
  // are also considered to be the same.  This allows keywords and raw lexed
  // identifiers with the same name to be treated the same.
  if (PPTok.getIdentifierInfo() &&
      PPTok.getIdentifierInfo() == RawTok.getIdentifierInfo())
    return true;

  return false;
}

/// Adapted from clang/lib/Rewrite/Frontend/RewriteMacros.cpp:
/// GetNextRawTok - Return the next raw token in the stream, skipping over
/// comments if ReturnComment is false.
static clang::Token const &
GetNextRawTok(std::vector<clang::Token> const &RawTokens,
              unsigned &CurTok,
              bool ReturnComment)
{
  assert(CurTok < RawTokens.size() && "Overran eof!");

  // If the client doesn't want comments and we have one, skip it.
  if (!ReturnComment && RawTokens[CurTok].is(clang::tok::comment))
    ++CurTok;

  return RawTokens[CurTok++];
}

/// Adapted from clang/lib/Rewrite/Frontend/RewriteMacros.cpp:
/// LexRawTokensFromMainFile - Lets all the raw tokens from the main file into
/// the specified vector.
static void LexRawTokensFromMainFile(clang::Preprocessor &PP,
                                     std::vector<clang::Token> &RawTokens)
{
  clang::SourceManager &SM = PP.getSourceManager();

  // Create a lexer to lex all the tokens of the main file in raw mode.  Even
  // though it is in raw mode, it will not return comments.
  const llvm::MemoryBuffer *FromFile = SM.getBuffer(SM.getMainFileID());
  clang::Lexer RawLex(SM.getMainFileID(), FromFile, SM, PP.getLangOpts());

  // Switch on comment lexing because we really do want them.
  RawLex.SetCommentRetentionState(true);

  clang::Token RawTok;
  do {
    RawLex.LexFromRawLexer(RawTok);

    // If we have an identifier with no identifier info for our raw token, look
    // up the indentifier info.  This is important for equality comparison of
    // identifier tokens.
    if (RawTok.is(clang::tok::raw_identifier))
      PP.LookUpIdentifierInfo(RawTok);

    RawTokens.push_back(RawTok);
  } while (RawTok.isNot(clang::tok::eof));
}

/// \brief Helper class used to build a FormattedStmt.
///
class FormattedStmtBuilder final
{
  /// The type of a "location ID". This is something that we can use to
  /// uniquely identify a \c clang::SourceLocation, that will be valid when we
  /// take it from our own \c clang::SourceManager and use it with the
  /// \c MappedAST 's original \c clang::SourceManager. Currently we rely on
  /// the fact that the two different \c clang::SourceManager 's are exposed
  /// to exactly the same information with the same settings, and thus we can
  /// use the raw encodings of the \c clang::SourceLocation s.
  typedef unsigned LocationIDTy;

  /// \brief Represents the position of a single \c clang::Token in \c Code.
  struct TokenPosition
  {
    std::string::size_type Start;
    unsigned Length;
  };

  /// \brief A position in the \c Code located from a \c clang::SourceLocation.
  struct LocatedPosition
  {
    TokenPosition const *TP;
    bool Hidden;
    bool Ambiguous;
  };

  /// Represents the type of a position that is being mapped.
  enum class PositionType {
    None,  ///< The type is unimportant.
    Start, ///< This position is the start location of a \c clang::Stmt.
    End    ///< This position is the end location of a \c clang::Stmt.
  };

  /// The top-level \c clang::Stmt that is being formatted.
  clang::Stmt const * const Stmt;

  /// The \c MappedAST that \c Stmt belongs to.
  seec::seec_clang::MappedAST const &MappedAST;

  /// Used by \c clang::Preprocessor::getSpelling().
  llvm::SmallString<256> TokenCleanBuffer;

  /// The formatted code.
  std::string Code;

  /// Mapping from source locations to locations in \c Code.
  std::multimap<LocationIDTy, TokenPosition> SourceLocationMap;

  /// \brief Create a "location ID" for the given \c clang::SourceLocation.
  ///
  LocationIDTy createLocationID(clang::SourceLocation Loc,
                                clang::SourceManager const &SM) const
  {
    // We're relying on the two different SourceManager's having been exposed
    // to the exact same information with the exact same settings, so that we
    // can rely on the raw encodings to be the same.
    return Loc.getRawEncoding();
  }

  /// \brief Add a \c clang::Token to the \c Code.
  /// \return The start of the \c clang::Token 's spelling in the \c Code.
  ///
  std::string::size_type addToken(clang::Token const &Tok,
                                  clang::Preprocessor const &PP)
  {
    if (Tok.hasLeadingSpace() && !Code.empty())
      Code += ' ';

    auto const StartPos = Code.size();
    Code += PP.getSpelling(Tok, TokenCleanBuffer);

    return StartPos;
  }

  /// \brief Map a \c clang::SourceLocation to the formatted \c Code.
  ///
  LocatedPosition getPosition(clang::SourceManager const &SM,
                              clang::SourceLocation const SLoc,
                              PositionType const PosType) const
  {
    auto const LocID = createLocationID(SLoc, SM);
    auto const MappedLocs = SourceLocationMap.equal_range(LocID);

    switch (std::distance(MappedLocs.first, MappedLocs.second)) {
      case 0:
        break;
      case 1:
        return LocatedPosition{&(MappedLocs.first->second), false, false};
      default:
        return LocatedPosition{&(MappedLocs.first->second), false, true};
    }

    // We didn't find any mapping for the real source location, so it must have
    // been expanded from a macro that wasn't expanded by the formatter (i.e.
    // it was defined in a system header).
    if (SM.isMacroArgExpansion(SLoc)) {
      auto const SpellingID = createLocationID(SM.getSpellingLoc(SLoc), SM);
      auto const Locs = SourceLocationMap.equal_range(SpellingID);

      switch (std::distance(Locs.first, Locs.second)) {
        case 0:
          // The spelling of this argument may be in a system header, and thus
          // not mapped. Fall down to the handling of macro bodies below, which
          // will attempt to get the (hidden) expansion location instead.
          break;
        case 1:
          return LocatedPosition{&(Locs.first->second), false, false};
        default:
          return LocatedPosition{&(Locs.first->second), false, true};
      }
    }

    // Macro body. If we're getting an end position, then get the last token
    // of the unexpanded macro (so that we cover the full area of function-like
    // macros, rather than just the identifier).
    auto const ExpRange = SM.getExpansionRange(SLoc);
    auto const ExpSLoc = (PosType != PositionType::End) ? ExpRange.first
                                                        : ExpRange.second;
    if (!ExpSLoc.isValid())
      return LocatedPosition{nullptr, true, false};

    auto const ExpID = createLocationID(ExpSLoc, SM);
    auto const Locs = SourceLocationMap.equal_range(ExpID);

    switch (std::distance(Locs.first, Locs.second)) {
      case 0:
        return LocatedPosition{nullptr, true, false};
      case 1:
        return LocatedPosition{&(Locs.first->second), true, false};
      default:
        return LocatedPosition{&(Locs.first->second), true, true};
    }
  }

  /// \brief Map a \c clang::Stmt to its location in the formatted \c Code.
  ///
  bool mapStmt(clang::Stmt const *S,
               llvm::DenseMap<clang::Stmt const *, FormattedStmtRange> &Ranges)
  {
    if (!S)
      return true;

    auto const &SM = MappedAST.getASTUnit().getSourceManager();
    auto const LocPosStart = getPosition(SM, S->getLocStart(),
                                         PositionType::Start);
    auto const LocPosEnd   = getPosition(SM, S->getLocEnd(),
                                         PositionType::End);

    if (LocPosStart.TP && LocPosEnd.TP) {
      auto const Start = LocPosStart.TP->Start;
      auto const End   = LocPosEnd.TP->Start + LocPosEnd.TP->Length;
      Ranges.insert(std::make_pair(S, FormattedStmtRange{Start,
                                                         End - Start,
                                                         LocPosStart.Hidden,
                                                         LocPosEnd.Hidden}));
    }

    for (auto const Child : S->children())
      mapStmt(Child, Ranges);

    return true;
  }

public:
  /// \brief Construct a new \c FormattedStmtBuilder for the given
  ///        \c clang::Stmt.
  /// \param ForStmt the top-level \c clang::Stmt that is being formatted.
  /// \param WithMappedAST the AST that \c ForStmt belongs to.
  ///
  FormattedStmtBuilder(clang::Stmt const * const ForStmt,
                       seec::seec_clang::MappedAST const &WithMappedAST)
  : Stmt(ForStmt),
    MappedAST(WithMappedAST)
  {}

  /// \brief Add a \c clang::Token that is not part of a macro expansion.
  ///
  void addRawToken(clang::Token const &Tok, clang::Preprocessor const &PP)
  {
    auto const CodeStartPos = addToken(Tok, PP);
    auto const &SM = PP.getSourceManager();
    auto const LocID = createLocationID(Tok.getLocation(), SM);
    SourceLocationMap.insert(std::make_pair(LocID,
                                            TokenPosition{CodeStartPos,
                                                          Tok.getLength()}));
  }

  /// \brief Add a \c clang::Token that is part of an unexpanded macro (i.e. the
  ///        macro was defined in a system header).
  ///
  void addUnexpandedToken(clang::Token const &Tok,
                          clang::Preprocessor const &PP)
  {
    auto const CodeStartPos = addToken(Tok, PP);
    auto const &SM = PP.getSourceManager();
    auto const LocID = createLocationID(Tok.getLocation(), SM);
    SourceLocationMap.insert(std::make_pair(LocID,
                                            TokenPosition{CodeStartPos,
                                                          Tok.getLength()}));
  }

  /// \brief Add a \c clang::Token that is part of an expanded macro (i.e. the
  ///        macro was not defined in a system header).
  ///
  void addExpandedToken(clang::Token const &Tok, clang::Preprocessor const &PP)
  {
    auto const CodeStartPos = addToken(Tok, PP);
    auto const &SM = PP.getSourceManager();
    auto const LocID = createLocationID(Tok.getLocation(), SM);
    SourceLocationMap.insert(std::make_pair(LocID,
                                            TokenPosition{CodeStartPos,
                                                          Tok.getLength()}));
  }

  /// \brief Complete the process and create a \c FormattedStmt.
  ///
  FormattedStmt finish()
  {
    // Map the Stmt's source locations to the code string.
    llvm::DenseMap<clang::Stmt const *, FormattedStmtRange> Ranges;
    mapStmt(Stmt, Ranges);

    return FormattedStmt{std::move(Code), std::move(Ranges)};
  }
};

/// \brief Create a new \c clang::CompilerInstance setup with the same
///        compilation options and source files used for \c MappedAST.
///
static std::unique_ptr<clang::CompilerInstance>
makeCompilerInstance(seec::seec_clang::MappedAST const &MappedAST)
{
  auto Diags = seec::makeUnique<clang::IgnoringDiagConsumer>();

  auto Clang = seec::makeUnique<clang::CompilerInstance>();
  Clang->createDiagnostics(Diags.release(), /* ShouldOwnClient */ true);

  auto &CompileInfo = MappedAST.getCompileInfo();
  auto CI = CompileInfo.createCompilerInvocation(Clang->getDiagnostics());
  if (!CI)
    return nullptr;

  Clang->setInvocation(CI.release());

  Clang->setTarget(
    clang::TargetInfo::CreateTargetInfo(Clang->getDiagnostics(),
                                        &Clang->getTargetOpts()));
  if (!Clang->hasTarget())
    return nullptr;

  Clang->createFileManager();
  Clang->createSourceManager(Clang->getFileManager());
  CompileInfo.createVirtualFiles(Clang->getFileManager(),
                                 Clang->getSourceManager());

  Clang->createPreprocessor();

  auto const MainFileName = CompileInfo.getMainFileName();
  auto const MainFile = Clang->getFileManager().getFile(MainFileName);
  Clang->getSourceManager().createMainFileID(MainFile);

  return Clang;
}

/// \brief Create a \c FormattedStmt by pretty-printing the \c clang::Stmt,
///        if we can't use the regular system.
///
static FormattedStmt prettyPrintFallback(clang::Stmt const *S,
                                         clang::ASTContext const &AST)
{
  clang::LangOptions LangOpts = AST.getLangOpts();

  clang::PrintingPolicy Policy(LangOpts);
  Policy.Indentation = 0;
  Policy.Bool = true;
  Policy.ConstantArraySizeAsWritten = true;

  std::string Print;
  llvm::raw_string_ostream Stream{Print};
  auto const Ranges = printStmtAndRecordRanges(Stream, S, Policy);
  Stream.flush();

  llvm::DenseMap<clang::Stmt const *, FormattedStmtRange> FormattedRanges;

  for (auto const &Range : Ranges)
    FormattedRanges.insert(std::make_pair(Range.first,
                                          FormattedStmtRange{
                                            Range.second.getStart(),
                                            Range.second.getLength(),
                                            /* hidden */ false,
                                            /* hidden */ false}));

  return FormattedStmt{std::move(Print), std::move(FormattedRanges)};
}

FormattedStmt formatStmtSource(clang::Stmt const *S,
                               seec::seec_clang::MappedAST const &MappedAST)
{
  FormattedStmtBuilder Builder{S, MappedAST};

  auto &ASTUnit = MappedAST.getASTUnit();
  auto &AST     = ASTUnit.getASTContext();
  auto &SrcMgr  = ASTUnit.getSourceManager();

  auto LocStart = S->getLocStart();
  auto LocEnd   = S->getLocEnd();
  if (!LocStart.isValid() || !LocEnd.isValid()) {
    llvm::errs() << "Falling back to pretty-printing due to invalid locs.\n";
    return prettyPrintFallback(S, AST);
  }

  if (LocStart.isMacroID())
    LocStart = SrcMgr.getExpansionLoc(LocStart);

  if (LocEnd.isMacroID())
    LocEnd = SrcMgr.getExpansionLoc(LocEnd);

  // For the cases that aren't handled by this formatting system, fall back to
  // the old pretty-printing method. At the moment we don't handle source ranges
  // that aren't part of the main file. This shouldn't be too troublesome for
  // students, because they shouldn't be defining functions in headers anyway.
  if (!SrcMgr.isWrittenInSameFile(LocStart, LocEnd)
      || !SrcMgr.isInMainFile(LocStart))
  {
    llvm::errs() << "Falling back to pretty-printing.\n";
    return prettyPrintFallback(S, AST);
  }

  // Determine the position that follows the end token (so that we can get the
  // complete text of the token).
  auto const LocPostEnd =
    clang::Lexer::getLocForEndOfToken(LocEnd, 0, SrcMgr, AST.getLangOpts());

  auto const StartOffset = SrcMgr.getFileOffset(LocStart);
  auto const EndOffset = LocPostEnd.isValid() ? SrcMgr.getFileOffset(LocPostEnd)
                                              : SrcMgr.getFileOffset(LocEnd);

  auto Clang = makeCompilerInstance(MappedAST);
  auto &PP = Clang->getPreprocessor();
  auto &SM = PP.getSourceManager();

  std::vector<clang::Token> RawTokens;
  LexRawTokensFromMainFile(PP, RawTokens);
  unsigned CurRawTok = 0;
  clang::Token RawTok = GetNextRawTok(RawTokens, CurRawTok, false);

  // Get the first preprocessing token.
  PP.EnterMainSourceFile();
  clang::Token PPTok;
  PP.Lex(PPTok);

  while (RawTok.isNot(clang::tok::eof) || PPTok.isNot(clang::tok::eof)) {
    auto PPLoc = SM.getExpansionLoc(PPTok.getLocation());

    // If PPTok is from a different source file, ignore it.
    if (!SM.isWrittenInMainFile(PPLoc)) {
      PP.Lex(PPTok);
      continue;
    }

    // Skip preprocessor directives.
    if (RawTok.is(clang::tok::hash) && RawTok.isAtStartOfLine()) {
      do {
        RawTok = GetNextRawTok(RawTokens, CurRawTok, false);
      } while (!RawTok.isAtStartOfLine() && RawTok.isNot(clang::tok::eof));
      continue;
    }

    // Okay, both tokens are from the same file.  Get their offsets from the
    // start of the file.
    unsigned PPOffs = SM.getFileOffset(PPLoc);
    unsigned RawOffs = SM.getFileOffset(RawTok.getLocation());

    // If the offsets are the same and the token kind is the same, ignore them.
    if (PPOffs == RawOffs && isSameToken(RawTok, PPTok)) {
      if (PPOffs >= EndOffset)
        break;

      if (StartOffset <= PPOffs)
        Builder.addRawToken(PPTok, PP);

      RawTok = GetNextRawTok(RawTokens, CurRawTok, false);
      PP.Lex(PPTok);
      continue;
    }

    // The PP token is farther along than the raw token. This occurs when we
    // expand a macro (because the PP token is the first expanded token, but
    // the raw token is sitting at the macro identifier). If the macro was
    // defined in a system header, or is a builtin, then we want to use the raw
    // tokens (to hide the implementation from the students) - otherwise use
    // the preprocessed tokens (to show the students their macro at work).
    if (RawOffs <= PPOffs) {
      bool ExpandMacro = true;

      if (auto II = RawTok.getIdentifierInfo()) {
        if (auto MI = PP.getMacroInfo(II)) {
          auto DefLoc = MI->getDefinitionLoc();
          if (MI->isBuiltinMacro() || SM.isInSystemHeader(DefLoc))
            ExpandMacro = false;
        }
      }

      if (ExpandMacro) {
        // We just move past the raw tokens here. The actual expansion will be
        // performed by the if branch farther down.
        do {
          RawTok = GetNextRawTok(RawTokens, CurRawTok, true);
          RawOffs = SM.getFileOffset(RawTok.getLocation());

          if (RawTok.is(clang::tok::comment)) {
            // Skip past the comment.
            RawTok = GetNextRawTok(RawTokens, CurRawTok, false);
            break;
          }
        } while (RawOffs <= PPOffs && !RawTok.isAtStartOfLine() &&
                 (PPOffs != RawOffs || !isSameToken(RawTok, PPTok)));

        continue;
      }
      else {
        // Skip preprocessed tokens from the macro's expansion.
        auto Loc = PPTok.getLocation();
        while (SM.isMacroArgExpansion(Loc) || SM.isMacroBodyExpansion(Loc)) {
          PP.Lex(PPTok);
          Loc = PPTok.getLocation();
        }

        PPLoc = SM.getExpansionLoc(PPTok.getLocation());
        PPOffs = SM.getFileOffset(PPLoc);

        // Now add the raw tokens that the macro was expanded from.
        do {
          if (StartOffset <= RawOffs)
            Builder.addUnexpandedToken(RawTok, PP);
          RawTok = GetNextRawTok(RawTokens, CurRawTok, true);
          RawOffs = SM.getFileOffset(RawTok.getLocation());
        } while (RawOffs < PPOffs);
      }
    }

    // Otherwise, there was a replacement an expansion.  Insert the new token
    // in the output buffer.  Insert the whole run of new tokens at once to get
    // them in the right order.
    if (PPOffs < RawOffs) {
      while (PPOffs < RawOffs) {
        if (StartOffset <= PPOffs && PPOffs <= EndOffset)
          Builder.addExpandedToken(PPTok, PP);
        PP.Lex(PPTok);
        PPLoc = SM.getExpansionLoc(PPTok.getLocation());
        PPOffs = SM.getFileOffset(PPLoc);
      }
    }
  }

  return Builder.finish();
}

} // namespace seec
