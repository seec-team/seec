//===- lib/Trace/FunctionState.cpp ----------------------------------------===//
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

#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {


//===------------------------------------------------------------------------===
// AllocaState
//===------------------------------------------------------------------------===

llvm::AllocaInst const *AllocaState::getInstruction() const {
  auto &Lookup = Parent->getFunctionLookup();
  auto Inst = Lookup.getInstruction(InstructionIndex);
  assert(Inst && llvm::isa<llvm::AllocaInst>(Inst));
  return llvm::cast<llvm::AllocaInst>(Inst);
}

MemoryStateRegion AllocaState::getMemoryRegion() const {
  auto &Thread = Parent->getParent();
  auto &Process = Thread.getParent();
  auto &Memory = Process.getMemory();
  return Memory.getRegion(MemoryArea(Address, getTotalSize()));
}


//===------------------------------------------------------------------------===
// RuntimeErrorState
//===------------------------------------------------------------------------===

llvm::Instruction const *RuntimeErrorState::getInstruction() const {
  auto Index = Parent.getFunctionLookup();
  return Index.getInstruction(InstructionIndex);
}

bool RuntimeErrorState::isActive() const {
  return ThreadTime == getParent().getParent().getThreadTime();
}


//===------------------------------------------------------------------------===
// FunctionState
//===------------------------------------------------------------------------===

FunctionState::FunctionState(ThreadState &Parent,
                             uint32_t Index,
                             FunctionIndex const &Function,
                             FunctionTrace Trace)
: Parent(&Parent),
  FunctionLookup(Parent.getParent().getModule().getFunctionIndex(Index)),
  Index(Index),
  Trace(Trace),
  ActiveInstruction(),
  ActiveInstructionComplete(false),
  InstructionValues(Function.getInstructionCount()),
  Allocas(),
  ParamByVals(),
  RuntimeErrors()
{
  assert(FunctionLookup);
}

llvm::Function const *FunctionState::getFunction() const {
  return Parent->getParent().getModule().getFunction(Index);
}

llvm::Instruction const *FunctionState::getInstruction(uint32_t Index) const {
  if (Index >= InstructionValues.size())
    return nullptr;
  
  return FunctionLookup->getInstruction(Index);
}

llvm::Instruction const *FunctionState::getActiveInstruction() const {
  if (!ActiveInstruction.assigned())
    return nullptr;

  return FunctionLookup->getInstruction(ActiveInstruction.get<0>());
}

seec::Maybe<MemoryArea>
FunctionState::getContainingMemoryArea(uintptr_t Address) const {
  auto const Alloca = getAllocaContaining(Address);
  if (Alloca)
    return MemoryArea(Alloca->getAddress(), Alloca->getTotalSize());
  
  for (auto const &ParamByVal : ParamByVals)
    if (ParamByVal.getArea().contains(Address))
      return ParamByVal.getArea();
  
  return seec::Maybe<MemoryArea>();
}

RuntimeValue const *
FunctionState::getCurrentRuntimeValue(uint32_t Index) const {
  assert(Index < InstructionValues.size());
  
  // If we have jumped to a prior Instruction, we consider the latter
  // Instruction values to no longer exist.
  if (ActiveInstruction.assigned<uint32_t>()) {
    auto const Active = ActiveInstruction.get<uint32_t>();
    if (Active < Index || (!ActiveInstructionComplete && Active == Index))
      return nullptr;
  }
  
  return &InstructionValues[Index];
}

RuntimeValue const *
FunctionState::getCurrentRuntimeValue(llvm::Instruction const *I) const {
  auto const MaybeIndex = FunctionLookup->getIndexOfInstruction(I);
  if (!MaybeIndex.assigned())
    return nullptr;
  
  return getCurrentRuntimeValue(MaybeIndex.get<0>());
}

std::vector<std::reference_wrapper<AllocaState const>>
FunctionState::getVisibleAllocas() const {
  std::vector<std::reference_wrapper<AllocaState const>> RetVal;
  
  if (!ActiveInstruction.assigned(0))
    return RetVal;
  
  auto const ActiveIdx = ActiveInstruction.get<0>();
  
  for (auto const &Alloca : Allocas) {
    auto const Inst = Alloca.getInstruction();
    auto const MaybeIdx = FunctionLookup->getIndexOfDbgDeclareFor(Inst);
    
    // If the index of the llvm.dbg.declare is greater than our active index,
    // then do not show this alloca. If the llvm.dbg.declare is the very next
    // instruction, then we should still show this (hence the ActiveIdx + 1).
    if (MaybeIdx.assigned(0) && MaybeIdx.get<0>() > ActiveIdx + 1)
      continue;
    
    RetVal.emplace_back(Alloca);
  }
  
  return RetVal;
}

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

void
FunctionState::
addRuntimeError(std::unique_ptr<seec::runtime_errors::RunError> Error) {
  assert(ActiveInstruction.assigned(0)
         && "Runtime error with no active instruction.");
  
  RuntimeErrors.emplace_back(*this,
                             ActiveInstruction.get<0>(),
                             std::move(Error),
                             getParent().getThreadTime());
}

void FunctionState::removeLastRuntimeError() {
  assert(!RuntimeErrors.empty() && "No runtime error to remove.");
  
  RuntimeErrors.pop_back();
}

seec::Maybe<seec::MemoryArea>
FunctionState::getParamByValArea(llvm::Argument const *Arg) const
{
  for (auto const &PBV : ParamByVals)
    if (PBV.getArgument() == Arg)
      return PBV.getArea();
  
  return seec::Maybe<seec::MemoryArea>();
}

void FunctionState::addByValArea(unsigned ArgumentNumber,
                                 uintptr_t Address,
                                 std::size_t Size)
{
  auto const Fn = getFunction();
  assert(ArgumentNumber < Fn->arg_size());
  
  auto ArgIt = Fn->arg_begin();
  std::advance(ArgIt, ArgumentNumber);
  
  ParamByVals.emplace_back(&*ArgIt, MemoryArea(Address, Size));
}

void FunctionState::removeByValArea(uintptr_t Address)
{
  auto const It = std::find_if(ParamByVals.begin(),
                               ParamByVals.end(),
                               [=] (ParamByValState const &P) {
                                  return P.getArea().contains(Address);
                               });
  
  if (It != ParamByVals.end())
    ParamByVals.erase(It);
}


/// Print a textual description of a FunctionState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State) {
  Out << "  Function [Index=" << State.getIndex() << "]\n";

  Out << "   Allocas:\n";
  for (auto const &Alloca : State.getAllocas()) {
    Out << "    " << Alloca.getInstructionIndex()
        <<  " =[" << Alloca.getElementCount()
        <<    "x" << Alloca.getElementSize()
        <<  "] @" << Alloca.getAddress()
        << "\n";
  }

  Out << "   Instruction values [Active=";
  if (State.getActiveInstructionIndex().assigned(0))
    Out << State.getActiveInstructionIndex().get<0>();
  else
    Out << "unassigned";
  Out << "]:\n";
  
  auto const InstructionCount = State.getInstructionCount();
  for (std::size_t i = 0; i < InstructionCount; ++i) {
    auto const Value = State.getCurrentRuntimeValue(i);
    if (!Value || !Value->assigned())
      continue;
    
    auto Type = State.getInstruction(i)->getType();
    
    Out << "    " << i << " = ";
    
    if (llvm::isa<llvm::IntegerType>(Type)) {
      Out << "(int64_t)" << getAs<int64_t>(*Value, Type)
          << ", (uint64_t)" << getAs<uint64_t>(*Value, Type);
    }
    else if (Type->isFloatTy()) {
      Out << "(float)" << getAs<float>(*Value, Type);
    }
    else if (Type->isDoubleTy()) {
      Out << "(double)" << getAs<double>(*Value, Type);
    }
    else if (Type->isPointerTy()) {
      Out << "(? *)" << getAs<void *>(*Value, Type);
    }
    else {
      Out << "(unknown type)";
    }
    
    Out << "\n";
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
