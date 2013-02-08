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


namespace seec {

namespace seec_clang {


class SearchingASTVisitor
: public clang::RecursiveASTVisitor<SearchingASTVisitor>
{
  clang::SourceManager &SMgr;
  
  clang::SourceLocation SLoc;
  
  clang::Decl *FoundDecl;
  
  clang::Stmt *FoundStmt;
  
  SearchResult::EFoundKind FoundLast;
  
public:
  SearchingASTVisitor(clang::SourceManager &SrcMgr,
                      clang::SourceLocation SearchLocation)
  : SMgr(SrcMgr),
    SLoc(SearchLocation),
    FoundDecl(nullptr),
    FoundStmt(nullptr),
    FoundLast(SearchResult::EFoundKind::None)
  {}
  
  bool VisitStmt(clang::Stmt *S) {
    auto Range = S->getSourceRange();
    if (Range.isInvalid())
      return true;
    
    if (SMgr.isBeforeInTranslationUnit(SLoc, Range.getBegin())) {
      return false;
    }
    
    if (SMgr.isBeforeInTranslationUnit(Range.getEnd(), SLoc)) {
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


seec::util::Maybe<SearchResult, seec::Error>
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
    if (Decl->getSourceRange().isInvalid())
      continue;
    
    // isInLexicalContext?
    
    if (clang::TagDecl *TD = llvm::dyn_cast<clang::TagDecl>(Decl))
      if (!TD->isFreeStanding())
        continue;
    
    // If Decl is before SLoc, continue.
    // If Decl is after SLoc, break.
    
    SearchingASTVisitor Visitor (SourceMgr, SLoc);
    Visitor.TraverseDecl(Decl);
    
    if (Visitor.getFoundLast() != SearchResult::EFoundKind::None)
      return SearchResult(Visitor.getFoundDecl(),
                          Visitor.getFoundStmt(),
                          Visitor.getFoundLast());
                        
  }
  
  return SearchResult(nullptr, nullptr, SearchResult::EFoundKind::None);
}


} // namespace seec_clang (in seec)

} // namespace seec
