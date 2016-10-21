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
#include "llvm/ADT/STLExtras.h"
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
  auto Diags = llvm::make_unique<clang::IgnoringDiagConsumer>();

  auto Clang = llvm::make_unique<clang::CompilerInstance>();
  Clang->createDiagnostics(Diags.release(), /* ShouldOwnClient */ true);

  auto &CompileInfo = MappedAST.getCompileInfo();
  auto CI = CompileInfo.createCompilerInvocation(Clang->getDiagnostics());
  if (!CI)
    return nullptr;

  Clang->setInvocation(CI.release());

  Clang->setTarget(
    clang::TargetInfo::CreateTargetInfo(
      Clang->getDiagnostics(),
      std::make_shared<clang::TargetOptions>(Clang->getTargetOpts())));

  if (!Clang->hasTarget())
    return nullptr;

  Clang->createFileManager();
  Clang->createSourceManager(Clang->getFileManager());
  CompileInfo.createVirtualFiles(Clang->getFileManager(),
                                 Clang->getSourceManager());

  Clang->createPreprocessor(clang::TranslationUnitKind::TU_Complete);

  auto const MainFileName = CompileInfo.getMainFileName();
  auto const MainFile = Clang->getFileManager().getFile(MainFileName);

  auto &SM = Clang->getSourceManager();
  SM.setMainFileID(SM.createFileID(MainFile,
                                   clang::SourceLocation(),
                                   clang::SrcMgr::C_User));

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

class FormattedStmtCacheForAST
{
  seec::seec_clang::MappedAST const &MappedAST;

  std::unique_ptr<clang::CompilerInstance> Clang;

  std::vector<clang::Token> PreprocessedTokens;

  std::map<clang::FileID, std::vector<clang::Token>> RawTokens;

public:
  FormattedStmtCacheForAST(seec::seec_clang::MappedAST const &ForMappedAST)
  : MappedAST(ForMappedAST),
    Clang(makeCompilerInstance(MappedAST)),
    PreprocessedTokens(),
    RawTokens()
  {
    // Generate all the preprocessed tokens. The raw tokens will be generated
    // lazily.
    auto &PP = Clang->getPreprocessor();

    PP.EnterMainSourceFile();
    clang::Token PPTok;

    do {
      PP.Lex(PPTok);
      PreprocessedTokens.push_back(PPTok);
    } while (PPTok.isNot(clang::tok::eof));
  }

  seec::seec_clang::MappedAST const &getMappedAST() const { return MappedAST; }

  bool hasCompilerInstance() const { return Clang != nullptr; }

  clang::Preprocessor const &getPreprocessor() const {
    return Clang->getPreprocessor();
  }

  std::vector<clang::Token> const &getPreprocessedTokens() const {
    return PreprocessedTokens;
  }

  std::vector<clang::Token> const *getRawTokens(clang::FileID const FID)
  {
    auto const It = RawTokens.lower_bound(FID);
    if (It != RawTokens.end() && It->first == FID)
      return &(It->second);

    // Generate the raw tokens now.
    auto &PP = Clang->getPreprocessor();
    auto &SM = PP.getSourceManager();

    bool BufferError = false;
    auto const Buffer = SM.getBuffer(FID, &BufferError);
    if (BufferError)
      return nullptr;

    std::vector<clang::Token> Tokens;
    clang::Lexer RawLex(FID, Buffer, SM, PP.getLangOpts());

    clang::Token RawTok;

    do {
      RawLex.LexFromRawLexer(RawTok);
      if (RawTok.is(clang::tok::raw_identifier))
        PP.LookUpIdentifierInfo(RawTok);
      Tokens.push_back(RawTok);
    } while (RawTok.isNot(clang::tok::eof));

    auto const Inserted = RawTokens.emplace_hint(It, FID, std::move(Tokens));
    return &(Inserted->second);
  }
};

static clang::Token const *getNextToken(std::vector<clang::Token> const &Tokens,
                                        std::size_t &TokenIndex)
{
  if (TokenIndex >= Tokens.size())
    return nullptr;

  return &(Tokens[TokenIndex++]);
}

bool formatMacro(FormattedStmtBuilder &Builder,
                 FormattedStmtCacheForAST &CacheForAST,
                 clang::Preprocessor const &PP,
                 clang::SourceManager &SM,
                 clang::SourceLocation PPLoc,
                 std::vector<clang::Token> const &PPTokens,
                 std::size_t &PPTokIdx,
                 clang::Token const *&PPTok)
{
  auto const ExpRange = SM.getExpansionRange(PPLoc);
  auto const OffStart = SM.getFileOffset(ExpRange.first);
  auto const OffEnd   = SM.getFileOffset(ExpRange.second);
  auto const FileID   = SM.getFileID(ExpRange.first);

  // We should expand this macro, because it's user-defined.
  if (!SM.isInSystemMacro(PPLoc)) {
    do {
      Builder.addExpandedToken(*PPTok, PP);
      PPTok = getNextToken(PPTokens, PPTokIdx);
      PPLoc = PPTok->getLocation();
    } while (SM.getFileOffset(SM.getExpansionLoc(PPLoc)) <= OffEnd);

    return true;
  }

  // Consume all the preprocessed tokens from this macro expansion.
  // TODO: Ensure that we don't cross over multiple raw files. If we do,
  // then fall back to the pretty-printing method (this shouldn't happen
  // anyway, so don't go to the trouble of supporting it).
  do {
    PPTok = getNextToken(PPTokens, PPTokIdx);
    PPLoc = PPTok->getLocation();
  } while (SM.getFileOffset(SM.getExpansionLoc(PPLoc)) <= OffEnd);

  // Add all the raw tokens for the code that this macro was expanded from.
  auto const RawTokens = CacheForAST.getRawTokens(FileID);
  if (!RawTokens)
    return false;

  std::size_t RawTokIdx = 0;
  clang::Token const *RawTok = getNextToken(*RawTokens, RawTokIdx);

  while (RawTok && RawTok->isNot(clang::tok::eof)) {
    auto const RawOff = SM.getFileOffset(RawTok->getLocation());
    if (RawOff > OffEnd)
      break;
    if (RawOff >= OffStart)
      Builder.addUnexpandedToken(*RawTok, PP);
    RawTok = getNextToken(*RawTokens, RawTokIdx);
  }

  return true;
}

FormattedStmt formatStmtSource(clang::Stmt const *S,
                               seec::seec_clang::MappedAST const &MappedAST)
{
  auto LocStart = S->getLocStart();
  auto LocEnd   = S->getLocEnd();
  if (!LocStart.isValid() || !LocEnd.isValid())
    return prettyPrintFallback(S, MappedAST.getASTUnit().getASTContext());

  FormattedStmtBuilder Builder{S, MappedAST};
  FormattedStmtCacheForAST CacheForAST{MappedAST};

  auto &PP = CacheForAST.getPreprocessor();
  auto &SM = PP.getSourceManager();

  auto const &PPTokens = CacheForAST.getPreprocessedTokens();
  std::size_t PPTokIdx = 0;
  clang::Token const *PPTok = getNextToken(PPTokens, PPTokIdx);

  while (PPTok && PPTok->isNot(clang::tok::eof)) {
    auto PPLoc = PPTok->getLocation();

    // Use SrcMgr because it has already parsed the entire TU, so this works.
    if (!SM.isBeforeInTranslationUnit(PPLoc, LocStart)
        && !SM.isBeforeInTranslationUnit(LocEnd, PPLoc))
    {
      if (PPLoc.isMacroID()) {
        auto const Success = formatMacro(Builder, CacheForAST, PP, SM, PPLoc,
                                         PPTokens, PPTokIdx, PPTok);
        if (!Success)
          return prettyPrintFallback(S, MappedAST.getASTUnit().getASTContext());

        // formatMacro() will leave PPTok as the next token following the macro
        // expansion, but this may be a new macro expansion, so check it above.
        continue;
      }

      // Use this token as-is.
      Builder.addRawToken(*PPTok, PP);
    }

    PPTok = getNextToken(PPTokens, PPTokIdx);
  }

  return Builder.finish();
}

} // namespace seec
