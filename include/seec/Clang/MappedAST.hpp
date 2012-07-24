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
#include "llvm/ADT/OwningPtr.h"
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
  llvm::OwningPtr<clang::ASTUnit> AST;
  std::vector<clang::Decl const *> Decls;
  std::vector<clang::Stmt const *> Stmts;

  MappedAST(MappedAST const &Other); // do not implement
  MappedAST & operator=(MappedAST const &RHS); // do not implement

  MappedAST(clang::ASTUnit *AST,
            std::vector<clang::Decl const *> &&Decls,
            std::vector<clang::Stmt const *> &&Stmts)
  : AST(AST),
    Decls(Decls),
    Stmts(Stmts)
  {}

public:
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


///
class MappedModule {
  llvm::Module const &Module;

  llvm::StringRef ExecutablePath;

  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags;

  llvm::DenseMap<llvm::MDNode const *, MappedAST const *> ASTLookup;

  std::vector<std::unique_ptr<MappedAST>> ASTList;

  unsigned MDStmtIdxKind;
  unsigned MDDeclIdxKind;

  MappedModule(MappedModule const &Other) = delete;
  MappedModule &operator=(MappedModule const &RHS) = delete;

public:
  /// Constructor.
  /// \param Module the llvm::Module to map.
  /// \param ExecutablePath Used by the Clang driver to find resources.
  /// \param Diags The diagnostics engine to use during compilation.
  MappedModule(llvm::Module const &Module,
               llvm::StringRef ExecutablePath,
               llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags)
  : Module(Module),
    ExecutablePath(ExecutablePath),
    Diags(Diags),
    ASTLookup(),
    ASTList(),
    MDStmtIdxKind(Module.getMDKindID(MDStmtIdxStr)),
    MDDeclIdxKind(Module.getMDKindID(MDDeclIdxStr))
  {}

  MappedAST const *getASTForFile(llvm::MDNode const *FileNode);

  clang::Decl const *getDecl(llvm::Instruction const *I);

  std::pair<clang::Decl const *, MappedAST const *>
  getDeclAndMappedAST(llvm::Instruction const *I);

  clang::Stmt const *getStmt(llvm::Instruction const *I);

  std::pair<clang::Stmt const *, MappedAST const *>
  getStmtAndMappedAST(llvm::Instruction const *I);
};

} // namespace clang (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDAST_HPP
