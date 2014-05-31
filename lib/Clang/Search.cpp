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
  
public:
  SearchingASTVisitor(clang::ASTContext const &WithAST,
                      clang::SourceManager &SrcMgr,
                      clang::SourceLocation SearchLocation)
  : AST(WithAST),
    SMgr(SrcMgr),
    SLoc(SearchLocation),
    FoundDecl(nullptr),
    FoundStmt(nullptr),
    FoundLast(SearchResult::EFoundKind::None)
  {}
  
  bool VisitStmt(clang::Stmt *S) {
    auto const Range = S->getSourceRange();
    if (Range.isInvalid())
      return true;
    
    auto const ExpBegin = SMgr.getExpansionLoc(Range.getBegin());
    if (SMgr.isBeforeInTranslationUnit(SLoc, ExpBegin)) {
      return false;
    }
    
    auto const ExpEnd = SMgr.getExpansionRange(Range.getEnd()).second;
    auto const CharEnd =
      clang::Lexer::getLocForEndOfToken(ExpEnd, 1, SMgr, AST.getLangOpts());

    if (SMgr.isBeforeInTranslationUnit(CharEnd, SLoc)) {
      return true;
    }
    
    FoundStmt = S;
    FoundLast = SearchResult::EFoundKind::Stmt;
    return true;
  }
  
  bool VisitDecl(clang::Decl *D) {
    auto Range = D->getSourceRange();
    if (Range.isInvalid())
      return true;
    
    if (SMgr.isBeforeInTranslationUnit(SLoc, Range.getBegin())) {
      return false;
    }
    
    if (SMgr.isBeforeInTranslationUnit(Range.getEnd(), SLoc)) {
      return true;
    }
    
    FoundDecl = D;
    FoundLast = SearchResult::EFoundKind::Decl;
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
