//===- lib/Clang/MappedFunctionState.cpp ----------------------------------===//
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

#include "seec/Clang/MappedAllocaState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Util/Printing.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclFriend.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/StmtObjC.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// Add all the visible VarDecl children of a Decl.
//===----------------------------------------------------------------------===//

void addVarDeclsVisible(clang::Decl const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{}

//===----------------------------------------------------------------------===//
// Add all the visible VarDecl children of a Stmt.
//===----------------------------------------------------------------------===//

void addVarDeclsVisible(clang::DeclStmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  if (Parent->isSingleDecl()) {
    auto const Decl = Parent->getSingleDecl();
    if (auto const VarDecl = llvm::dyn_cast<clang::VarDecl>(Decl)) {
      Set.insert(VarDecl);
    }
  }
  else {
  
  }
}

void addVarDeclsVisible(clang::Stmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{}

//===----------------------------------------------------------------------===//
// Find all visible VarDecls from a given location.
//===----------------------------------------------------------------------===//

void getVarDeclsVisible(clang::Decl const *FromDecl,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set);

void getVarDeclsVisible(clang::Stmt const *FromStmt,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set);

void getVarDeclsVisible(clang::Decl const *FromDecl,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  // Add to Set based on the dynamic type of FromDecl.
  switch (FromDecl->getKind()) {
#define DECL(DERIVED, BASE)                                                    \
    case ::clang::Decl::Kind::DERIVED:                                         \
      addVarDeclsVisible(llvm::cast< ::clang::DERIVED##Decl >(FromDecl),       \
                         PriorToDecl, PriorToStmt, Map, Set);                  \
      break;
#define ABSTRACT_DECL(DECL)
#include "clang/AST/DeclNodes.inc"
  }
  
  // Continue searching above/prior to this point in the AST.
  auto const Parent = Map.getParent(FromDecl);
  
  if (Parent.assigned<clang::Decl const *>())
    getVarDeclsVisible(Parent.get<clang::Decl const *>(),
                       FromDecl,
                       nullptr,
                       Map,
                       Set);
  else if (Parent.assigned<clang::Stmt const *>())
    getVarDeclsVisible(Parent.get<clang::Stmt const *>(),
                       FromDecl,
                       nullptr,
                       Map,
                       Set);
}

void getVarDeclsVisible(clang::Stmt const *FromStmt,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  // Add to Set based on the dynamic type of FromStmt.
  switch (FromStmt->getStmtClass()) {
    case ::clang::Stmt::StmtClass::NoStmtClass:
      break;
    
#define STMT(CLASS, PARENT)                                                    \
    case ::clang::Stmt::StmtClass::CLASS##Class:                               \
      addVarDeclsVisible(llvm::cast< ::clang::CLASS >(FromStmt),               \
                         PriorToDecl, PriorToStmt, Map, Set);                  \
      break;
#define ABSTRACT_STMT(STMT)
#include "clang/AST/StmtNodes.inc"
  }
  
  // Continue searching above/prior to this point in the AST.
  auto const Parent = Map.getParent(FromStmt);
  
  if (Parent.assigned<clang::Decl const *>())
    getVarDeclsVisible(Parent.get<clang::Decl const *>(),
                       nullptr,
                       FromStmt,
                       Map,
                       Set);
  else if (Parent.assigned<clang::Stmt const *>())
    getVarDeclsVisible(Parent.get<clang::Stmt const *>(),
                       nullptr,
                       FromStmt,
                       Map,
                       Set);
}

llvm::DenseSet<clang::VarDecl const *>
getVarDeclsVisibleFrom(clang::Decl const *Decl,
                       seec::seec_clang::MappedAST const &Map)
{
  llvm::DenseSet<clang::VarDecl const *> Set;
  getVarDeclsVisible(Decl, nullptr, nullptr, Map, Set);
  return Set;
}

llvm::DenseSet<clang::VarDecl const *>
getVarDeclsVisibleFrom(clang::Stmt const *Stmt,
                       seec::seec_clang::MappedAST const &Map)
{
  llvm::DenseSet<clang::VarDecl const *> Set;
  getVarDeclsVisible(Stmt, nullptr, nullptr, Map, Set);
  return Set;
}

//===----------------------------------------------------------------------===//
// FunctionState
//===----------------------------------------------------------------------===//

FunctionState::FunctionState(ThreadState &WithParent,
                             seec::trace::FunctionState &ForUnmappedState)
: Parent(WithParent),
  UnmappedState(ForUnmappedState),
  Parameters(),
  Variables(),
  RuntimeErrors()
{
  auto const &Trace = Parent.getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  
  // Add allocas (parameters and variables).
  for (auto const AllocaRef : UnmappedState.getVisibleAllocas()) {
    seec::trace::AllocaState const &RawAlloca = AllocaRef;
    auto const AllocaInst = RawAlloca.getInstruction();
    auto const Mapping = MappedModule.getMapping(AllocaInst);
    if (!Mapping.getAST())
      continue;
    
    auto const Decl = Mapping.getDecl();
    if (!Decl)
      continue;
    
    // TODO: Check if this Decl is still in scope.
    
    if (auto const ParmVar = llvm::dyn_cast< ::clang::ParmVarDecl>(Decl)) {
      Parameters.emplace_back(*this, RawAlloca, ParmVar);
    }
    else if (auto const Var = llvm::dyn_cast< ::clang::VarDecl>(Decl)) {
      Variables.emplace_back(*this, RawAlloca, Var);
    }
  }
  
  // Add runtime errors.
  for (auto const &ErrorState : UnmappedState.getRuntimeErrors()) {
    RuntimeErrors.emplace_back(*this, ErrorState);
  }
}

FunctionState::~FunctionState() = default;

void FunctionState::print(llvm::raw_ostream &Out,
                          seec::util::IndentationGuide &Indentation) const
{
  Out << Indentation.getString()
      << "Function \"" << this->getNameAsString() << "\"\n";
  
  // Parameters.
  Out << Indentation.getString() << "Parameters:\n";
  {
    Indentation.indent();
    
    for (auto const &Alloca : Parameters)
      Alloca.print(Out, Indentation);
    
    Indentation.unindent();
  }
  
  // Local variables.
  Out << Indentation.getString() << "Local variables:\n";
  {
    Indentation.indent();
    
    for (auto const &Alloca : Variables)
      Alloca.print(Out, Indentation);
    
    Indentation.unindent();
  }
  
  // Runtime errors.
  if (!RuntimeErrors.empty()) {
    Out << Indentation.getString() << "Runtime errors:\n";
    {
      Indentation.indent();
      
      for (auto const &Error : RuntimeErrors)
        Error.print(Out, Indentation);
      
      Indentation.unindent();
    }
  }
}

//===----------------------------------------------------------------------===//
// Accessors.
//===----------------------------------------------------------------------===//

::clang::FunctionDecl const *FunctionState::getFunctionDecl() const {
  auto const &Trace = Parent.getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  auto const LLVMFunction = UnmappedState.getFunction();
  
  auto MappedDecl = MappedModule.getMappedFunctionDecl(LLVMFunction);
  if (!MappedDecl)
    return nullptr;
  
  return llvm::dyn_cast< ::clang::FunctionDecl>(MappedDecl->getDecl());
}

std::string FunctionState::getNameAsString() const {
  auto const FunctionDecl = getFunctionDecl();
  if (FunctionDecl)
    return FunctionDecl->getNameAsString();
  
  return UnmappedState.getFunction()->getName().str();
}


//===----------------------------------------------------------------------===//
// Stmt evaluation.
//===----------------------------------------------------------------------===//

::clang::Stmt const *FunctionState::getActiveStmt() const {
  auto const Instruction = UnmappedState.getActiveInstruction();
  if (!Instruction)
    return nullptr;
  
  auto const &Trace = Parent.getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  return MappedModule.getStmt(Instruction);
}

std::shared_ptr<Value const>
FunctionState::getStmtValue(::clang::Stmt const *S) const {
  return seec::cm::getValue(Parent.getParent().getCurrentValueStore(),
                            S,
                            Parent.getParent().getProcessTrace().getMapping(),
                            UnmappedState);
}


//===----------------------------------------------------------------------===//
// Local variables.
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// llvm::raw_ostream output
//===----------------------------------------------------------------------===//

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State)
{
  seec::util::IndentationGuide Indent("  ");
  State.print(Out, Indent);
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
