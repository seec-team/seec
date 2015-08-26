//===- lib/Clang/Search.cpp -----------------------------------------------===//
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

#include "seec/Clang/Search.hpp"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Lex/Lexer.h"


namespace seec {

namespace seec_clang {


class SearchingASTVisitor
: public clang::RecursiveASTVisitor<SearchingASTVisitor>
{
  clang::ASTContext const &AST;

  clang::SourceManager &SMgr;
  
  clang::SourceLocation SLoc;
  
  clang::Decl *FoundDecl;
  
  clang::Stmt *FoundStmt;
  
  SearchResult::EFoundKind FoundLast;

  clang::SourceLocation FoundLastLocBegin;

  clang::SourceLocation FoundLastLocEnd;

  enum class ELocationCheckResult {
    RangeInvalid,
    RangeBefore,
    RangeAfter,
    RangeCovers,
    ExpansionRangeCovers
  };

  ELocationCheckResult checkRange(clang::SourceRange const Range) const
  {
    if (Range.isInvalid())
      return ELocationCheckResult::RangeInvalid;

    // Handle expanded macro arguments.
    if (SMgr.isMacroArgExpansion(Range.getBegin())) {
      assert(Range.getBegin().isValid() && Range.getEnd().isValid());

      auto const SpellBegin = SMgr.getSpellingLoc(Range.getBegin());
      if (SMgr.isBeforeInTranslationUnit(SLoc, SpellBegin))
        return ELocationCheckResult::RangeAfter;

      auto const SpellEnd = SMgr.getSpellingLoc(Range.getEnd());
      auto const CharEnd =
        clang::Lexer::getLocForEndOfToken(SpellEnd, 1, SMgr, AST.getLangOpts());

      if (SMgr.isBeforeInTranslationUnit(CharEnd, SLoc))
        return ELocationCheckResult::RangeBefore;

      return ELocationCheckResult::RangeCovers;
    }

    // Handle expanded macro body.
    if (SMgr.isMacroBodyExpansion(Range.getBegin())) {
      assert(Range.getBegin().isValid() && Range.getEnd().isValid());

      auto const ExpBegin = SMgr.getExpansionLoc(Range.getBegin());
      if (SMgr.isBeforeInTranslationUnit(SLoc, ExpBegin))
        return ELocationCheckResult::RangeAfter;

      auto const ExpEnd = SMgr.getExpansionRange(Range.getEnd()).second;
      auto const CharEnd =
        clang::Lexer::getLocForEndOfToken(ExpEnd, 1, SMgr, AST.getLangOpts());

      if (SMgr.isBeforeInTranslationUnit(CharEnd, SLoc))
        return ELocationCheckResult::RangeBefore;

      return ELocationCheckResult::ExpansionRangeCovers;
    }

    // Handle regular locations.
    auto const SpellBegin = SMgr.getSpellingLoc(Range.getBegin());
    if (SMgr.isBeforeInTranslationUnit(SLoc, SpellBegin))
      return ELocationCheckResult::RangeAfter;

    auto const SpellEnd = SMgr.getSpellingLoc(Range.getEnd());
    auto const CharEnd =
      clang::Lexer::getLocForEndOfToken(SpellEnd, 1, SMgr, AST.getLangOpts());

    if (SMgr.isBeforeInTranslationUnit(CharEnd, SLoc))
      return ELocationCheckResult::RangeBefore;

    return ELocationCheckResult::RangeCovers;
  }
  
public:
  SearchingASTVisitor(clang::ASTContext const &WithAST,
                      clang::SourceManager &SrcMgr,
                      clang::SourceLocation SearchLocation)
  : AST(WithAST),
    SMgr(SrcMgr),
    SLoc(SearchLocation),
    FoundDecl(nullptr),
    FoundStmt(nullptr),
    FoundLast(SearchResult::EFoundKind::None),
    FoundLastLocBegin(),
    FoundLastLocEnd()
  {}
  
  bool VisitStmt(clang::Stmt *S) {
    switch (checkRange(S->getSourceRange())) {
      case ELocationCheckResult::RangeInvalid: return true;
      case ELocationCheckResult::RangeBefore:  return true;
      case ELocationCheckResult::RangeAfter:   return false;

      case ELocationCheckResult::RangeCovers:
        FoundStmt = S;
        FoundLast = SearchResult::EFoundKind::Stmt;
        FoundLastLocBegin = SMgr.getExpansionLoc(S->getLocStart());
        FoundLastLocEnd   = SMgr.getExpansionRange(S->getLocEnd()).second;
        return true;

      // Use outermost rather than innermost node for macro body expansions.
      case ELocationCheckResult::ExpansionRangeCovers:
        if (FoundLast == SearchResult::EFoundKind::None
            || FoundLastLocBegin != SMgr.getExpansionLoc(S->getLocStart())
            || FoundLastLocEnd != SMgr.getExpansionRange(S->getLocEnd()).second)
        {
          FoundStmt = S;
          FoundLast = SearchResult::EFoundKind::Stmt;
          FoundLastLocBegin = SMgr.getExpansionLoc(S->getLocStart());
          FoundLastLocEnd   = SMgr.getExpansionRange(S->getLocEnd()).second;
        }
        return true;
    }

    return true;
  }
  
  bool VisitDecl(clang::Decl *D) {
    switch (checkRange(D->getSourceRange())) {
      case ELocationCheckResult::RangeInvalid: return true;
      case ELocationCheckResult::RangeBefore:  return true;
      case ELocationCheckResult::RangeAfter:   return false;

      case ELocationCheckResult::RangeCovers:
        FoundDecl = D;
        FoundLast = SearchResult::EFoundKind::Decl;
        FoundLastLocBegin = SMgr.getExpansionLoc(D->getLocStart());
        FoundLastLocEnd   = SMgr.getExpansionRange(D->getLocEnd()).second;
        return true;

      // Use outermost rather than innermost node for macro body expansions.
      case ELocationCheckResult::ExpansionRangeCovers:
        if (FoundLast == SearchResult::EFoundKind::None
            || FoundLastLocBegin != SMgr.getExpansionLoc(D->getLocStart()))
        {
          FoundDecl = D;
          FoundLast = SearchResult::EFoundKind::Decl;
          FoundLastLocBegin = SMgr.getExpansionLoc(D->getLocStart());
          FoundLastLocEnd   = SMgr.getExpansionRange(D->getLocEnd()).second;
        }
        return true;
    }

    return true;
  }
  
  clang::Decl *getFoundDecl() const { return FoundDecl; }
  
  clang::Stmt *getFoundStmt() const { return FoundStmt; }
  
  SearchResult::EFoundKind getFoundLast() const { return FoundLast; }
};


seec::Maybe<SearchResult, seec::Error>
searchImpl(clang::ASTUnit &AST,
           clang::SourceManager &SourceMgr,
           clang::FileID FileID,
           clang::SourceLocation SLoc)
{
  if (SLoc.isInvalid())
    return seec::Error(LazyMessageByRef::create("SeeCClang",
                                                {"errors",
                                                 "SourceLocationInvalid"}));
  
  llvm::SmallVector< ::clang::Decl *, 8> FoundDecls;
  AST.findFileRegionDecls(FileID,
                          SourceMgr.getFileOffset(SLoc),
                          0,
                          FoundDecls);
  
  if (FoundDecls.empty())
    return SearchResult(nullptr, nullptr, SearchResult::EFoundKind::None);
  
  for (auto &Decl : FoundDecls) {
    auto const SrcRange = Decl->getSourceRange();
    if (SrcRange.isInvalid())
      continue;
    
    // isInLexicalContext?
    
    if (clang::TagDecl *TD = llvm::dyn_cast<clang::TagDecl>(Decl))
      if (!TD->isFreeStanding())
        continue;
    
    // Skip this Decl and keep searching.
    if (SourceMgr.isBeforeInTranslationUnit(SrcRange.getEnd(), SLoc))
      continue;

    // We've already passed the search location.
    if (SourceMgr.isBeforeInTranslationUnit(SLoc, SrcRange.getBegin()))
      break;
    
    SearchingASTVisitor Visitor (AST.getASTContext(), SourceMgr, SLoc);
    Visitor.TraverseDecl(Decl);
    
    if (Visitor.getFoundLast() != SearchResult::EFoundKind::None)
      return SearchResult(Visitor.getFoundDecl(),
                          Visitor.getFoundStmt(),
                          Visitor.getFoundLast());
  }
  
  return SearchResult(nullptr, nullptr, SearchResult::EFoundKind::None);
}

seec::Maybe<SearchResult, seec::Error>
search(::clang::ASTUnit &AST,
       llvm::StringRef Filename,
       unsigned Line,
       unsigned Column)
{
  auto &FileMgr = AST.getFileManager();
    
  auto const File = FileMgr.getFile(Filename, false);
  if (!File)
    return seec::Error(LazyMessageByRef::create("SeeCClang",
                                                {"errors",
                                                 "FileManagerGetFileFail"}));
  
  auto &SourceMgr = AST.getSourceManager();
  auto const FileID = SourceMgr.translateFile(File);
  auto const SLoc = SourceMgr.translateLineCol(FileID, Line, Column);
  
  return searchImpl(AST, SourceMgr, FileID, SLoc);
}

seec::Maybe<SearchResult, seec::Error>
search(::clang::ASTUnit &AST,
       llvm::StringRef Filename,
       unsigned Offset)
{
  auto &FileMgr = AST.getFileManager();
    
  auto const File = FileMgr.getFile(Filename, false);
  if (!File)
    return seec::Error(LazyMessageByRef::create("SeeCClang",
                                                {"errors",
                                                 "FileManagerGetFileFail"}));
  
  auto &SourceMgr = AST.getSourceManager();
  auto const FileID = SourceMgr.translateFile(File);
  auto const SLocStart = SourceMgr.getLocForStartOfFile(FileID);
  auto const SLoc = SLocStart.getLocWithOffset(Offset);
  
  return searchImpl(AST, SourceMgr, FileID, SLoc);
}


} // namespace seec_clang (in seec)

} // namespace seec
