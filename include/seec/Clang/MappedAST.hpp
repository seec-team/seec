//===- include/seec/clang/MappedAST.hpp -----------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file Support SeeC's indexing of Clang ASTs.
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MAPPEDAST_HPP
#define SEEC_CLANG_MAPPEDAST_HPP

#include "clang/Frontend/ASTUnit.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace clang {
  class Decl;
  class DiagnosticsEngine;
  class FileSystemOptions;
  class Stmt;
}

namespace seec {

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {


///
class MappedAST {
  clang::ASTUnit *AST;

  std::vector<clang::Decl const *> Decls;

  std::vector<clang::Stmt const *> Stmts;

  /// Constructor.
  MappedAST(clang::ASTUnit *AST,
            std::vector<clang::Decl const *> &&Decls,
            std::vector<clang::Stmt const *> &&Stmts)
  : AST(AST),
    Decls(Decls),
    Stmts(Stmts)
  {}

  // Don't allow copying.
  MappedAST(MappedAST const &Other) = delete;
  MappedAST & operator=(MappedAST const &RHS) = delete;

public:
  /// Destructor.
  ~MappedAST();

  /// Factory.
  static std::unique_ptr<MappedAST>
  FromASTUnit(clang::ASTUnit *AST);

  /// Factory.
  static std::unique_ptr<MappedAST>
  LoadFromASTFile(llvm::StringRef Filename,
                  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags,
                  clang::FileSystemOptions const &FileSystemOpts);

  /// Factory.
  static std::unique_ptr<MappedAST>
  LoadFromCompilerInvocation(
    std::unique_ptr<clang::CompilerInvocation> Invocation,
    llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags);
  
  
  /// \name Accessors
  /// @{

  /// Get the underlying ASTUnit.
  clang::ASTUnit &getASTUnit() const { return *AST; }
  
  /// Get all mapped clang::Decl pointers.
  decltype(Decls) const &getAllDecls() const { return Decls; }
  
  /// Get all mapped clang::Stmt pointers.
  decltype(Stmts) const &getAllStmts() const { return Stmts; }

  /// Get the clang::Decl at the given index.
  clang::Decl const *getDeclFromIdx(uint64_t DeclIdx) const {
    if (DeclIdx < Decls.size())
      return Decls[DeclIdx];
    return nullptr;
  }

  /// Get the clang::Stmt at the given index.
  clang::Stmt const *getStmtFromIdx(uint64_t StmtIdx) const {
    if (StmtIdx < Stmts.size())
      return Stmts[StmtIdx];
    return nullptr;
  }
  
  /// @} (Accessors)
};


} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDAST_HPP
