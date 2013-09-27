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

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"


using namespace clang;
using namespace llvm;


namespace seec {

namespace seec_clang {


//===----------------------------------------------------------------------===//
// class Mapper
//===----------------------------------------------------------------------===//

/// \brief Visit an AST and construct mapping information.
///
class MappingASTVisitor : public RecursiveASTVisitor<MappingASTVisitor> {
  typedef MappedAST::ASTNodeTy ASTNodeTy;
  
  /// Current stack of visitation.
  std::vector<ASTNodeTy> VisitStack;
  
  /// All Decls in visitation order.
  std::vector<Decl const *> Decls;
  
  /// All Stmts in visitation order.
  std::vector<Stmt const *> Stmts;
  
  /// Parents of Decls.
  llvm::DenseMap<clang::Decl const *, ASTNodeTy> DeclParents;
  
  /// Parents of Stmts.
  llvm::DenseMap<clang::Stmt const *, ASTNodeTy> StmtParents;

public:
  /// \brief Constructor.
  ///
  MappingASTVisitor()
  : Decls(),
    Stmts()
  {}
  
  /// \name Accessors.
  /// @{
  
  /// Get all Decls in visitation order.
  decltype(Decls) &getDecls() { return Decls; }
  
  /// Get all Stmts in visitation order.
  decltype(Stmts) &getStmts() { return Stmts; }
  
  /// Get all Decl parents.
  decltype(DeclParents) &getDeclParents() { return DeclParents; }
  
  /// Get all Stmt parents.
  decltype(StmtParents) &getStmtParents() { return StmtParents; }
  
  /// @}


  /// \name RecursiveASTVisitor Methods
  /// @{
  
  /// \brief Return whether \param S should be traversed using data recursion
  /// to avoid a stack overflow with extreme cases.
  ///
  bool shouldUseDataRecursionFor(Stmt *S) const {
    return false;
  }
  
  /// \brief Traverse a Stmt.
  ///
  bool TraverseStmt(::clang::Stmt *S) {
    ::clang::Stmt const * const CS = S;
    
    // Fill in the parent for this Stmt, if any.
    if (!VisitStack.empty()) {
      auto const &Parent = VisitStack.back();
      if (Parent.assigned())
        StmtParents.insert(std::make_pair(CS, Parent));
    }
    
    // Traverse this Stmt using the default implementation.
    VisitStack.emplace_back(CS);
    auto const Ret = RecursiveASTVisitor<MappingASTVisitor>::TraverseStmt(S);
    VisitStack.pop_back();
    return Ret;
  }
  
  /// \brief Traverse a Decl.
  ///
  bool TraverseDecl(::clang::Decl *D) {
    ::clang::Decl const * const CD = D;
    
    // Fill in the parent for this Decl, if any.
    if (!VisitStack.empty()) {
      auto const &Parent = VisitStack.back();
      if (Parent.assigned())
        DeclParents.insert(std::make_pair(CD, Parent));
    }
    
    // Traverse this Decl using the default implementation.
    VisitStack.push_back(CD);
    auto const Ret = RecursiveASTVisitor<MappingASTVisitor>::TraverseDecl(D);
    VisitStack.pop_back();
    return Ret;
  }
  
  /// \brief Visit a Decl.
  ///
  bool VisitDecl(::clang::Decl *D) {
    Decls.push_back(D);
    return true;
  }
  
  /// \brief Visit a Stmt.
  ///
  bool VisitStmt(::clang::Stmt *S) {
    Stmts.push_back(S);
    return true;
  }
  
  /// @}
};

//===----------------------------------------------------------------------===//
// class MappedAST
//===----------------------------------------------------------------------===//

MappedAST::MappedAST(clang::ASTUnit *ForAST,
                     MappingASTVisitor &&WithMapping)
: AST(ForAST),
  Decls(std::move(WithMapping.getDecls())),
  Stmts(std::move(WithMapping.getStmts())),
  DeclParents(std::move(WithMapping.getDeclParents())),
  StmtParents(std::move(WithMapping.getStmtParents()))
{}

MappedAST::~MappedAST() {
  delete AST;
}

std::unique_ptr<MappedAST>
MappedAST::FromASTUnit(clang::ASTUnit *AST) {
  if (!AST)
    return nullptr;

  std::vector<Decl const *> Decls;
  std::vector<Stmt const *> Stmts;

  MappingASTVisitor Mapper;

  for (auto It = AST->top_level_begin(), End = AST->top_level_end();
       It != End; ++It) {
    Mapper.TraverseDecl(*It);
  }

  auto Mapped = std::unique_ptr<MappedAST>(new MappedAST(AST,
                                                         std::move(Mapper)));
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

bool MappedAST::isParent(::clang::Decl const *Parent,
                         ::clang::Decl const *Child) const
{
  auto const DP = getParent(Child);
  
  if (DP.assigned<clang::Decl const *>()) {
    auto const DPDecl = DP.get<clang::Decl const *>();
    return DPDecl == Parent ? true : isParent(Parent, DPDecl);
  }
  else if (DP.assigned<clang::Stmt const *>()) {
    auto const DPStmt = DP.get<clang::Stmt const *>();
    return isParent(Parent, DPStmt);
  }
  
  return false;
}

bool MappedAST::isParent(::clang::Decl const *Parent,
                         ::clang::Stmt const *Child) const
{
  auto const DP = getParent(Child);
  
  if (DP.assigned<clang::Decl const *>()) {
    auto const DPDecl = DP.get<clang::Decl const *>();
    return DPDecl == Parent ? true : isParent(Parent, DPDecl);
  }
  else if (DP.assigned<clang::Stmt const *>()) {
    auto const DPStmt = DP.get<clang::Stmt const *>();
    return isParent(Parent, DPStmt);
  }
  
  return false;
}


} // namespace seec_clang (in seec)

} // namespace seec
