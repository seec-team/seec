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
  
  /// The AST that we are visiting.
  clang::ASTUnit &AST;
  
  /// The SourceManager for this AST.
  clang::SourceManager &SourceManager;
  
  /// All Decls in visitation order.
  std::vector<Decl const *> Decls;
  
  /// All Stmts in visitation order.
  std::vector<Stmt const *> Stmts;

  /// All Decls seen.
  llvm::DenseSet<Decl const *> DeclsSeen;

  /// All Stmts seen.
  llvm::DenseSet<Stmt const *> StmtsSeen;
  
  /// All Decls that are referred to by non-system code.
  llvm::DenseSet<clang::Decl const *> DeclsReferenced;

  /// All VariableArrayType types in visitation order.
  std::vector<::clang::VariableArrayType *> VATypes;

public:
  /// \brief Constructor.
  ///
  MappingASTVisitor(clang::ASTUnit &ForAST)
  : AST(ForAST),
    SourceManager(ForAST.getSourceManager()),
    Decls(),
    Stmts(),
    DeclsSeen(),
    StmtsSeen(),
    DeclsReferenced(),
    VATypes()
  {}
  
  /// \name Accessors.
  /// @{
  
  /// Get all Decls in visitation order.
  decltype(Decls) &getDecls() { return Decls; }
  
  /// Get all Stmts in visitation order.
  decltype(Stmts) &getStmts() { return Stmts; }
  
  /// Get all Decls that are referenced by non-system code.
  decltype(DeclsReferenced) &getDeclsReferenced() { return DeclsReferenced; }
  
  /// @}


  /// \name Mutators.
  /// @{

  /// \brief Revisits VariableArrayType's size expressions.
  ///
  void revisitVariableArrayTypeSizeExprs() {
    for (auto const VAType : VATypes)
      TraverseStmt(VAType->getSizeExpr());
  }

  /// @}


  /// \name RecursiveASTVisitor Methods
  /// @{
  
  /// \brief Return whether \param S should be traversed using data recursion
  /// to avoid a stack overflow with extreme cases.
  ///
  bool shouldUseDataRecursionFor(Stmt *S) const {
    return false;
  }
  
  /// \brief Visit a Decl.
  ///
  bool VisitDecl(::clang::Decl *D) {
    if (DeclsSeen.insert(D).second)
      Decls.push_back(D);
    return true;
  }
  
  /// \brief Visit a Stmt.
  ///
  bool VisitStmt(::clang::Stmt *S) {
    if (StmtsSeen.insert(S).second)
      Stmts.push_back(S);
    return true;
  }
  
  /// \brief Visit a DeclRefExpr.
  ///
  bool VisitDeclRefExpr(::clang::DeclRefExpr *DR) {
    if (DR && !SourceManager.isInSystemHeader(DR->getLocation())) {
      if (auto const TheDecl = DR->getDecl()) {
        auto const Canon = TheDecl->getCanonicalDecl();
        DeclsReferenced.insert(Canon);
      }
    }
    
    return true;
  }

  /// \brief Visit a VariableArrayType.
  ///
  bool VisitVariableArrayType(::clang::VariableArrayType *T) {
    VATypes.push_back(T);
    return true;
  }
  
  /// \brief Print Decl kinds and Stmt classes in visitation order.
  ///
  void printInVisitationOrder() const
  {
    llvm::errs() << "decls:\n";
    for (auto const D : Decls)
      llvm::errs() << "  " << D->getDeclKindName() << "\n";

    llvm::errs() << "stmts:\n";
    for (auto const S : Stmts)
      llvm::errs() << "  " << S->getStmtClassName() << "\n";
  }

  /// @}
};

//===----------------------------------------------------------------------===//
// class MappedAST
//===----------------------------------------------------------------------===//

MappedAST::MappedAST(MappedCompileInfo const &FromCompileInfo,
                     clang::ASTUnit *ForAST,
                     MappingASTVisitor WithMapping)
: CompileInfo(FromCompileInfo),
  AST(ForAST),
  Decls(std::move(WithMapping.getDecls())),
  Stmts(std::move(WithMapping.getStmts())),
  DeclsReferenced(std::move(WithMapping.getDeclsReferenced()))
{}

MappedAST::~MappedAST() {
  delete AST;
}

std::unique_ptr<MappedAST>
MappedAST::FromASTUnit(MappedCompileInfo const &FromCompileInfo,
                       clang::ASTUnit *AST)
{
  if (!AST)
    return nullptr;

  MappingASTVisitor Mapper {*AST};

  Mapper.TraverseDecl(AST->getASTContext().getTranslationUnitDecl());
  Mapper.revisitVariableArrayTypeSizeExprs();

#if defined(SEEC_DEBUG_NODE_MAPPING)
  Mapper.printInVisitationOrder();
#endif

  auto Mapped = std::unique_ptr<MappedAST>(new MappedAST(FromCompileInfo,
                                                         AST,
                                                         std::move(Mapper)));
  if (!Mapped)
    delete AST;

  return Mapped;
}

seec::Maybe<uint64_t> MappedAST::getIdxForDecl(clang::Decl const *Decl) const
{
  auto const It = std::find(Decls.begin(), Decls.end(), Decl);
  uint64_t const Idx = It != Decls.end() ? std::distance(Decls.begin(), It) : 0;
  return Idx;
}

seec::Maybe<uint64_t> MappedAST::getIdxForStmt(clang::Stmt const *Stmt) const
{
  auto const It = std::find(Stmts.begin(), Stmts.end(), Stmt);
  uint64_t const Idx = It != Stmts.end() ? std::distance(Stmts.begin(), It) : 0;
  return Idx;
}

bool MappedAST::contains(::clang::Decl const *Decl) const
{
  return &(Decl->getASTContext()) == &(AST->getASTContext());
}

bool MappedAST::contains(::clang::Stmt const *Stmt) const {
  for (auto const S : Stmts)
    if (S == Stmt)
      return true;
  return false;
}

static MappedAST::ASTNodeTy
getFirstParent(ArrayRef<ast_type_traits::DynTypedNode> const &Parents)
{
  if (!Parents.empty()) {
    auto const &Parent = Parents[0];
    
    if (auto const ParentDecl = Parent.get<clang::Decl>())
      return ParentDecl;
    else if (auto const ParentStmt = Parent.get<clang::Stmt>())
      return ParentStmt;
  }
  
  return MappedAST::ASTNodeTy();
}

MappedAST::ASTNodeTy MappedAST::getParent(clang::Decl const *D) const
{
  return getFirstParent(AST->getASTContext().getParents(*D));
}

MappedAST::ASTNodeTy MappedAST::getParent(clang::Stmt const *S) const
{
  return getFirstParent(AST->getASTContext().getParents(*S));
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
