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
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
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
    if (PriorToDecl && Decl == PriorToDecl)
      return;
    
    if (auto const VarDecl = llvm::dyn_cast<clang::VarDecl>(Decl))
      Set.insert(VarDecl);
  }
  else {
    for (auto const Decl : Parent->getDeclGroup()) {
      if (PriorToDecl && Decl == PriorToDecl)
        return;
      
      if (auto const VarDecl = llvm::dyn_cast<clang::VarDecl>(Decl))
        Set.insert(VarDecl);
    }
  }
}

void addVarDeclsVisible(clang::CompoundStmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  for (auto const Stmt : Parent->children()) {
    if (Stmt == PriorToStmt)
      return;
    
    if (auto const DeclStmt = llvm::dyn_cast<clang::DeclStmt>(Stmt))
      addVarDeclsVisible(DeclStmt, nullptr, nullptr, Map, Set);
  }
}

void addVarDeclsVisible(clang::ForStmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  // The initialisation statement.
  if (auto const Init = Parent->getInit()) {
    if (PriorToStmt && Init == PriorToStmt)
      return;
    
    if (auto const DeclStmt = llvm::dyn_cast<clang::DeclStmt>(Init))
      addVarDeclsVisible(DeclStmt, nullptr, nullptr, Map, Set);
  }
  
  // The condition statement.
  if (PriorToStmt && Parent->getCond() == PriorToStmt)
    return;
  
  if (auto const CV = Parent->getConditionVariable())
    Set.insert(CV);
  
  // The increment statement.
  if (PriorToStmt && Parent->getInc() == PriorToStmt)
    return;
  
  // Any VarDecls in the Body should have already been added.
}

void addVarDeclsVisible(clang::IfStmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  if (PriorToStmt && Parent->getCond() == PriorToStmt)
    return;
  
  if (PriorToStmt && Parent->getConditionVariableDeclStmt() == PriorToStmt)
    return;
  
  if (auto const CV = Parent->getConditionVariable())
    Set.insert(CV);
  
  // Any VarDecls in the Body should have already been added.
}

void addVarDeclsVisible(clang::SwitchStmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  if (PriorToStmt && Parent->getCond() == PriorToStmt)
    return;
  
  if (auto const CV = Parent->getConditionVariable())
    Set.insert(CV);
  
  // Any VarDecls in the Body should have already been added.
}

void addVarDeclsVisible(clang::WhileStmt const *Parent,
                        clang::Decl const *PriorToDecl,
                        clang::Stmt const *PriorToStmt,
                        seec::seec_clang::MappedAST const &Map,
                        llvm::DenseSet<clang::VarDecl const *> &Set)
{
  if (PriorToStmt && Parent->getCond() == PriorToStmt)
    return;
  
  if (auto const CV = Parent->getConditionVariable())
    Set.insert(CV);
  
  // Any VarDecls in the Body should have already been added.
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


//===----------------------------------------------------------------------===//
// FunctionState
//===----------------------------------------------------------------------===//

FunctionState::FunctionState(ThreadState &WithParent,
                             seec::trace::FunctionState &ForUnmappedState)
: Parent(WithParent),
  UnmappedState(ForUnmappedState),
  Mapping(Parent.getParent()
                .getProcessTrace()
                .getMapping()
                .getMappedFunctionDecl(UnmappedState.getFunction())),
  Parameters(),
  Variables(),
  RuntimeErrors()
{
  auto const &Trace = Parent.getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  
  // Get all visible VarDecls from this active node.
  llvm::DenseSet<clang::VarDecl const *> VisibleDecls;
  
  if (auto const ActiveStmt = this->getActiveStmt()) {
    if (auto const AST = MappedModule.getASTForStmt(ActiveStmt)) {
      getVarDeclsVisible(ActiveStmt, nullptr, nullptr, *AST, VisibleDecls);
    }
  }
  
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
    
    if (auto const ParmVar = llvm::dyn_cast< ::clang::ParmVarDecl>(Decl)) {
      Parameters.emplace_back(*this, RawAlloca, ParmVar);
    }
    else if (auto const Var = llvm::dyn_cast< ::clang::VarDecl>(Decl)) {
      // Check if this Decl is in scope.
      if (VisibleDecls.count(Var)) {
        Variables.emplace_back(*this, RawAlloca, Var);
      }
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
  
  // Active Stmt.
  if (auto const Stmt = this->getActiveStmt()) {
    auto const &Trace = Parent.getParent().getProcessTrace();
    auto const &MappedModule = Trace.getMapping();
    auto const MappedAST = MappedModule.getASTForStmt(Stmt);
    
    Out << Indentation.getString() << "Active statement: "
        << Stmt->getStmtClassName() << " at ";
    
    if (MappedAST) {
      auto const &SrcManager = MappedAST->getASTUnit().getSourceManager();
      auto const StartLoc = Stmt->getLocStart();
      
      auto const Filename = SrcManager.getFilename(StartLoc);
      auto const Line = SrcManager.getSpellingLineNumber(StartLoc);
      auto const Column = SrcManager.getSpellingColumnNumber(StartLoc);
      
      Out << Filename << " line " << Line << " column " << Column << "\n";
    }
    else {
      Out << "unknown location.\n";
    }
  }
  else {
    Out << Indentation.getString() << "No active statement.\n";
  }
  
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
  return Mapping ? llvm::dyn_cast< ::clang::FunctionDecl >(Mapping->getDecl())
                 : nullptr;
}

std::string FunctionState::getNameAsString() const {
  auto const FunctionDecl = getFunctionDecl();
  
  return FunctionDecl ? FunctionDecl->getNameAsString()
                      : UnmappedState.getFunction()->getName().str();
}

seec::seec_clang::MappedAST const *FunctionState::getMappedAST() const {
  return Mapping ? &Mapping->getAST()
                 : nullptr;
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
// Runtime errors.
//===----------------------------------------------------------------------===//

auto
FunctionState::getRuntimeErrorsActive() const
-> seec::Range<decltype(RuntimeErrors)::const_iterator>
{
  auto const It = std::find_if(RuntimeErrors.begin(), RuntimeErrors.end(),
                               [] (RuntimeErrorState const &Err) {
                                return Err.isActive();
                               });
  
  return seec::Range<decltype(RuntimeErrors)::const_iterator>
                    (It, RuntimeErrors.end());
}


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
