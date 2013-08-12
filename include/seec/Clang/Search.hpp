//===- include/seec/Clang/Search.hpp --------------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_SEARCH_HPP
#define SEEC_CLANG_SEARCH_HPP


#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/StringRef.h"


namespace clang {
  class ASTUnit;
  class Decl;
  class Stmt;
}


namespace seec {

namespace seec_clang {


class SearchResult {
public:
  enum class EFoundKind {
    None,
    Decl,
    Stmt
  };
  
private:
  ::clang::Decl *FoundDecl;
  
  ::clang::Stmt *FoundStmt;
  
  EFoundKind FoundLast;
  
public:
  SearchResult(::clang::Decl *Decl,
               ::clang::Stmt *Stmt,
               EFoundKind Last)
  : FoundDecl(Decl),
    FoundStmt(Stmt),
    FoundLast(Last)
  {}
  
  /// \name Accessors.
  /// @{
  
  ::clang::Decl *getFoundDecl() const { return FoundDecl; }
  
  ::clang::Stmt *getFoundStmt() const { return FoundStmt; }
  
  EFoundKind getFoundLast() const { return FoundLast; }
  
  /// @}
};


seec::Maybe<SearchResult, seec::Error>
search(::clang::ASTUnit &AST,
       llvm::StringRef Filename,
       unsigned Line,
       unsigned Column);

seec::Maybe<SearchResult, seec::Error>
search(::clang::ASTUnit &AST,
       llvm::StringRef Filename,
       unsigned Offset);


} // namespace seec_clang (in seec)

} // namespace seec


#endif // SEEC_CLANG_SEARCH_HPP
