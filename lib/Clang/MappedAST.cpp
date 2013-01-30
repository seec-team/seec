//===- lib/Clang/MappedAST.cpp --------------------------------------------===//
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

#include "seec/Clang/Compile.hpp"
#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedStmt.hpp"
#include "seec/Clang/MDNames.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/AST/RecursiveASTVisitor.h"

#include "llvm/Constants.h"
#include "llvm/Instruction.h"


using namespace clang;
using namespace llvm;


namespace seec {

namespace seec_clang {


//===----------------------------------------------------------------------===//
// class Mapper
//===----------------------------------------------------------------------===//

class MappingASTVisitor : public RecursiveASTVisitor<MappingASTVisitor> {
  std::vector<Decl const *> &Decls;
  std::vector<Stmt const *> &Stmts;

public:
  MappingASTVisitor(std::vector<Decl const *> &Decls,
                    std::vector<Stmt const *> &Stmts)
  : Decls(Decls),
    Stmts(Stmts)
  {}

  /// RecursiveASTVisitor Methods
  /// \{

  bool VisitDecl(Decl *D) {
    Decls.push_back(D);
    return true;
  }

  bool VisitStmt(Stmt *S) {
    Stmts.push_back(S);
    return true;
  }

  /// \}
};

//===----------------------------------------------------------------------===//
// class MappedAST
//===----------------------------------------------------------------------===//

MappedAST::~MappedAST() {
  delete AST;
}

std::unique_ptr<MappedAST>
MappedAST::FromASTUnit(clang::ASTUnit *AST) {
  if (!AST)
    return nullptr;

  std::vector<Decl const *> Decls;
  std::vector<Stmt const *> Stmts;

  MappingASTVisitor Mapper(Decls, Stmts);

  for (auto It = AST->top_level_begin(), End = AST->top_level_end();
       It != End; ++It) {
    Mapper.TraverseDecl(*It);
  }

  auto Mapped = std::unique_ptr<MappedAST>(new MappedAST(AST,
                                                         std::move(Decls),
                                                         std::move(Stmts)));
  if (!Mapped)
    delete AST;

  return Mapped;
}

std::unique_ptr<MappedAST>
MappedAST::LoadFromASTFile(llvm::StringRef Filename,
                           IntrusiveRefCntPtr<DiagnosticsEngine> Diags,
                           FileSystemOptions const &FileSystemOpts) {
  return MappedAST::FromASTUnit(
          ASTUnit::LoadFromASTFile(Filename.str(), Diags, FileSystemOpts));
}

std::unique_ptr<MappedAST>
MappedAST::LoadFromCompilerInvocation(
  std::unique_ptr<CompilerInvocation> Invocation,
  IntrusiveRefCntPtr<DiagnosticsEngine> Diags)
{
  return MappedAST::FromASTUnit(
          ASTUnit::LoadFromCompilerInvocation(Invocation.release(), Diags));
}


} // namespace seec_clang (in seec)

} // namespace seec
