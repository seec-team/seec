//===- MappedAST.hpp - Support SeeC's indexing of Clang ASTs --------------===//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MAPPEDAST_HPP
#define SEEC_CLANG_MAPPEDAST_HPP

#include "seec/Clang/MDNames.hpp"

#include "clang/Frontend/ASTUnit.h"

#include "llvm/Module.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

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

  /// Get the underlying ASTUnit.
  clang::ASTUnit const &getASTUnit() const { return *AST; }

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
};


/// Represents a mapping from an llvm::Function to a clang::Decl.
class MappedGlobalDecl {
  llvm::sys::Path FilePath;

  clang::Decl const *Decl;

  llvm::Function const *Function;

public:
  /// Constructor.
  MappedGlobalDecl(llvm::sys::Path FilePath,
                   clang::Decl const *Decl,
                   llvm::Function const *Function)
  : FilePath(FilePath),
    Decl(Decl),
    Function(Function)
  {}

  /// Copy constructor.
  MappedGlobalDecl(MappedGlobalDecl const &) = default;

  /// Copy assignment.
  MappedGlobalDecl &operator=(MappedGlobalDecl const &) = default;

  /// Get the path to the source file that this mapping refers to.
  llvm::sys::Path const &getFilePath() const { return FilePath; }

  /// Get the clang::Decl that is mapped to.
  clang::Decl const *getDecl() const { return Decl; }

  /// Get the llvm::Function that is mapped from.
  llvm::Function const *getFunction() const { return Function; }
};


///
class MappedModule {
  llvm::Module const &Module;

  llvm::StringRef ExecutablePath;

  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags;

  llvm::DenseMap<llvm::MDNode const *, MappedAST const *> ASTLookup;

  std::vector<std::unique_ptr<MappedAST>> ASTList;

  unsigned MDStmtIdxKind;

  unsigned MDDeclIdxKind;

  llvm::DenseMap<llvm::Function const *, MappedGlobalDecl> GlobalLookup;

  // Don't allow copying.
  MappedModule(MappedModule const &Other) = delete;
  MappedModule &operator=(MappedModule const &RHS) = delete;

  MappedAST const *getASTForFile(llvm::MDNode const *FileNode);

public:
  /// Constructor.
  /// \param Module the llvm::Module to map.
  /// \param ExecutablePath Used by the Clang driver to find resources.
  /// \param Diags The diagnostics engine to use during compilation.
  MappedModule(llvm::Module const &Module,
               llvm::StringRef ExecutablePath,
               llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags);

  /// Get the GlobalLookup.
  decltype(GlobalLookup) const &getGlobalLookup() const { return GlobalLookup; }

  /// Find the clang::Decl mapping for an llvm::Function, if one exists.
  MappedGlobalDecl const *getMappedGlobalDecl(llvm::Function const *F) const;

  /// Find the clang::Decl for an llvm::Function, if one exists.
  clang::Decl const *getDecl(llvm::Function const *F) const;

  /// For the given llvm::Instruction, find the clang::Decl.
  clang::Decl const *getDecl(llvm::Instruction const *I);

  /// For the given llvm::Instruction, find the clang::Decl and the MappedAST
  /// that it belongs to.
  std::pair<clang::Decl const *, MappedAST const *>
  getDeclAndMappedAST(llvm::Instruction const *I);

  /// For the given llvm::Instruction, find the clang::Stmt.
  clang::Stmt const *getStmt(llvm::Instruction const *I);

  /// For the given llvm::Instruction, find the clang::Stmt and the MappedAST
  /// that it belongs to.
  std::pair<clang::Stmt const *, MappedAST const *>
  getStmtAndMappedAST(llvm::Instruction const *I);
};

} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDAST_HPP
