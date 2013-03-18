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

#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Trace/FunctionState.hpp"

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
  UnmappedState(ForUnmappedState)
{
  
}

FunctionState::~FunctionState() = default;


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
  Out << "Function \"" << State.getNameAsString() << "\"\n";
  
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
