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
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Util/Printing.hpp"

#include "clang/AST/Decl.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// FunctionState
//===----------------------------------------------------------------------===//

FunctionState::FunctionState(ThreadState &WithParent,
                             seec::trace::FunctionState &ForUnmappedState)
: Parent(WithParent),
  UnmappedState(ForUnmappedState),
  Parameters(),
  Variables()
{
  auto const &Trace = Parent.getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  
  for (auto &RawAlloca : UnmappedState.getAllocas()) {
    auto const AllocaInst = RawAlloca.getInstruction();
    auto const Mapping = MappedModule.getMapping(AllocaInst);
    if (!Mapping.getAST())
      continue;
    
    // TODO: Check if this alloca had passed the debug declaration point
    //       (if one exists).
    
    auto const Decl = Mapping.getDecl();
    if (!Decl)
      continue;
    
    if (auto const ParmVar = llvm::dyn_cast< ::clang::ParmVarDecl>(Decl)) {
      Parameters.emplace_back(*this, RawAlloca, ParmVar);
    }
    else if (auto const Var = llvm::dyn_cast< ::clang::VarDecl>(Decl)) {
      Variables.emplace_back(*this, RawAlloca, Var);
    }
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
