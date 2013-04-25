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


#include "seec/Util/Maybe.hpp"

#include "clang/Frontend/ASTUnit.h"

#include "llvm/ADT/DenseMap.h"
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


class MappingASTVisitor;


/// \brief Provides indexing and lookup for clang::ASTUnit objects.
///
class MappedAST {
public:
  typedef seec::Maybe<clang::Decl const *, clang::Stmt const *> ASTNodeTy;
  
private:
  /// The ASTUnit that is mapped.
  clang::ASTUnit *AST;

  /// All known Decl pointers in visitation order.
  std::vector<clang::Decl const *> Decls;

  /// All known Stmt pointers in visitation order.
  std::vector<clang::Stmt const *> Stmts;
  
  /// Parents of Decls.
  llvm::DenseMap<clang::Decl const *, ASTNodeTy> DeclParents;
  
  /// Parents of Stmts.
  llvm::DenseMap<clang::Stmt const *, ASTNodeTy> StmtParents;
  
  /// \brief Constructor.
  MappedAST(clang::ASTUnit *ForAST,
            MappingASTVisitor &&WithMapping);

  // Don't allow copying.
  MappedAST(MappedAST const &Other) = delete;
  MappedAST & operator=(MappedAST const &RHS) = delete;

public:
  /// \brief Destructor.
  ///
  ~MappedAST();

  /// \brief Factory.
  ///
  static std::unique_ptr<MappedAST>
  FromASTUnit(clang::ASTUnit *AST);

  /// \brief Factory.
  ///
  static std::unique_ptr<MappedAST>
  LoadFromASTFile(llvm::StringRef Filename,
                  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags,
                  clang::FileSystemOptions const &FileSystemOpts);

  /// \brief Factory.
  ///
  static std::unique_ptr<MappedAST>
  LoadFromCompilerInvocation(
    std::unique_ptr<clang::CompilerInvocation> Invocation,
    llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags);
  
  
  /// \name Accessors
  /// @{

  /// \brief Get the underlying ASTUnit.
  ///
  clang::ASTUnit &getASTUnit() const { return *AST; }
  
  /// \brief Get all mapped clang::Decl pointers.
  ///
  decltype(Decls) const &getAllDecls() const { return Decls; }
  
  /// \brief Get all mapped clang::Stmt pointers.
  ///
  decltype(Stmts) const &getAllStmts() const { return Stmts; }

  /// \brief Get the clang::Decl at the given index.
  ///
  clang::Decl const *getDeclFromIdx(uint64_t DeclIdx) const {
    if (DeclIdx < Decls.size())
      return Decls[DeclIdx];
    return nullptr;
  }

  /// \brief Get the clang::Stmt at the given index.
  ///
  clang::Stmt const *getStmtFromIdx(uint64_t StmtIdx) const {
    if (StmtIdx < Stmts.size())
      return Stmts[StmtIdx];
    return nullptr;
  }
  
  /// \brief Check if this AST contains the given Decl.
  ///
  bool contains(::clang::Decl const *Decl) const {
    for (auto const D : Decls)
      if (D == Decl)
        return true;
    return false;
  }
  
  /// \brief Check if this AST contains the given Stmt.
  ///
  bool contains(::clang::Stmt const *Stmt) const {
    for (auto const S : Stmts)
      if (S == Stmt)
        return true;
    return false;
  }
  
  /// \brief Get the parent of a Decl, if it has one.
  ///
  ASTNodeTy getParent(::clang::Decl const *Decl) const {
    auto const It = DeclParents.find(Decl);
    if (It == DeclParents.end())
      return ASTNodeTy{};
    return It->second;
  }
  
  /// \brief Get the parent of a Stmt, if it has one.
  ///
  ASTNodeTy getParent(::clang::Stmt const *Stmt) const {
    auto const It = StmtParents.find(Stmt);
    if (It == StmtParents.end())
      return ASTNodeTy{};
    return It->second;
  }
  
  /// @} (Accessors)
};


} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDAST_HPP
