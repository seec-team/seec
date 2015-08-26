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

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace clang {
  class ASTUnit;
  class Decl;
  class DiagnosticsEngine;
  class FileSystemOptions;
  class Stmt;
}

namespace seec {

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {


class MappedCompileInfo;
class MappingASTVisitor;


/// \brief Provides indexing and lookup for clang::ASTUnit objects.
///
class MappedAST {
public:
  typedef seec::Maybe<clang::Decl const *, clang::Stmt const *> ASTNodeTy;
  
private:
  /// The compile information used to recreate this AST.
  MappedCompileInfo const &CompileInfo;

  /// The ASTUnit that is mapped.
  clang::ASTUnit * const AST;

  /// All known Decl pointers in visitation order.
  std::vector<clang::Decl const *> const Decls;

  /// All known Stmt pointers in visitation order.
  std::vector<clang::Stmt const *> const Stmts;
  
  /// All Decls that are referred to by non-system code.
  llvm::DenseSet<clang::Decl const *> const DeclsReferenced;
  
  /// \brief Constructor.
  MappedAST(MappedCompileInfo const &FromCompileInfo,
            clang::ASTUnit *ForAST,
            MappingASTVisitor WithMapping);

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
  FromASTUnit(MappedCompileInfo const &FromCompileInfo,
              clang::ASTUnit *AST);
  
  
  /// \name Accessors
  /// @{

  /// \brief Get the compilation information used for this AST.
  ///
  MappedCompileInfo const &getCompileInfo() const { return CompileInfo; }
  
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
  
  /// \brief Find the index for the given clang::Decl (if it exists).
  ///
  seec::Maybe<uint64_t> getIdxForDecl(clang::Decl const *Decl) const;
  
  /// \brief Find the index for the given clang::Stmt (if it exists).
  ///
  seec::Maybe<uint64_t> getIdxForStmt(clang::Stmt const *Stmt) const;
  
  /// \brief Check if this AST contains the given Decl.
  ///
  bool contains(::clang::Decl const *Decl) const;
  
  /// \brief Check if this AST contains the given Stmt.
  ///
  bool contains(::clang::Stmt const *Stmt) const;
  
  /// \brief Get the parent of a Decl, if it has one.
  ///
  ASTNodeTy getParent(::clang::Decl const *Decl) const;
  
  /// \brief Get the parent of a Stmt, if it has one.
  ///
  ASTNodeTy getParent(::clang::Stmt const *Stmt) const;
  
  /// \brief Check if a Decl is a parent of a Decl.
  ///
  bool isParent(::clang::Decl const *Parent, ::clang::Decl const *Child) const;
  
  /// \brief Check if a Decl is a parent of a Stmt.
  ///
  bool isParent(::clang::Decl const *Parent, ::clang::Stmt const *Child) const;
  
  /// \brief Check if a Decl is referenced by non-system code.
  ///
  bool isReferenced(::clang::Decl const *D) const {
    return DeclsReferenced.count(D);
  }
  
  /// @} (Accessors)
};


} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDAST_HPP
