//===- lib/Trace/TracedFunction.cpp ---------------------------------------===//
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

#include "seec/Trace/TracedFunction.hpp"
#include "seec/Trace/TraceMemory.hpp"
#include "seec/Trace/TraceThreadListener.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"

#define SEEC_DEBUG_PTROBJ 0


namespace seec {

namespace trace {


//===----------------------------------------------------------------------===//
// RecordedFunction
//===----------------------------------------------------------------------===//

void RecordedFunction::setCompletion(offset_uint const WithEventOffsetEnd,
                                     uint64_t const WithThreadTimeExited)
{
  assert(EventOffsetEnd == 0 && ThreadTimeExited == 0);
  EventOffsetEnd = WithEventOffsetEnd;
  ThreadTimeExited = WithThreadTimeExited;
}


//===----------------------------------------------------------------------===//
// Support getCurrentRuntimeValue.
//===----------------------------------------------------------------------===//

llvm::DataLayout const &TracedFunction::getDataLayout() const
{
  return ThreadListener.getDataLayout();
}

uintptr_t
TracedFunction::getRuntimeAddress(llvm::GlobalVariable const *GV) const
{
  return ThreadListener.getRuntimeAddress(GV);
}

uintptr_t TracedFunction::getRuntimeAddress(llvm::Function const *F) const
{
  return ThreadListener.getRuntimeAddress(F);
}


//===----------------------------------------------------------------------===//
// Accessors for active-only information.
//===----------------------------------------------------------------------===//

seec::Maybe<MemoryArea>
TracedFunction::getContainingMemoryArea(uintptr_t Address) const
{
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  if (Address < StackLow || Address > StackHigh) {
    // Not occupied by our stack, but may belong to a byval argument.
    for (auto const &Arg : ByValArgs) {
      if (Arg.getArea().contains(Address)) {
        return Arg.getArea();
      }
    }
  }
  else {
    // May be occupied by our stack.
    for (auto const &Alloca : Allocas) {
      auto AllocaArea = Alloca.area();
      if (AllocaArea.contains(Address)) {
        return AllocaArea;
      }
    }
  }
  
  return seec::Maybe<MemoryArea>();
}


//===----------------------------------------------------------------------===//
// byval argument memory area tracking.
//===----------------------------------------------------------------------===//

void TracedFunction::addByValArg(llvm::Argument const * const Arg,
                                 MemoryArea const &Area)
{
  auto const &Process = ThreadListener.getProcessListener();
  ArgPointerObjects[Arg] = Process.makePointerObject(Area.address());

  std::lock_guard<std::mutex> Lock(StackMutex);
  
  ByValArgs.emplace_back(Arg, Area);
}

seec::Maybe<seec::MemoryArea>
TracedFunction::getParamByValArea(llvm::Argument const *Arg) const
{
  std::lock_guard<std::mutex> Lock(StackMutex);

  for (auto const &PBV : ByValArgs)
    if (PBV.getArgument() == Arg)
      return PBV.getArea();

  return seec::Maybe<seec::MemoryArea>();
}


//===----------------------------------------------------------------------===//
// Pointer origin tracking.
//===----------------------------------------------------------------------===//

PointerTarget TracedFunction::getPointerObject(llvm::Argument const *A) const
{
  auto const It = ArgPointerObjects.find(A);
  return It != ArgPointerObjects.end() ? It->second : PointerTarget(0, 0);
}

void TracedFunction::setPointerObject(llvm::Argument const *A,
                                      PointerTarget const &Object)
{
  ArgPointerObjects[A] = Object;
#if SEEC_DEBUG_PTROBJ
  llvm::errs() << "set ptr " << Object << " for argument " << *A << "\n";
#endif
}

PointerTarget TracedFunction::getPointerObject(llvm::Instruction const *I) const
{
  auto const It = PointerObjects.find(I);
  auto const Obj = It != PointerObjects.end() ? It->second
                                              : PointerTarget(0, 0);
#if SEEC_DEBUG_PTROBJ
  llvm::errs() << "get ptr " << Obj << " for instruction " << *I << "\n";
#endif
  return Obj;
}

void TracedFunction::setPointerObject(llvm::Instruction const *I,
                                      PointerTarget const &Object)
{
  PointerObjects[I] = Object;
#if SEEC_DEBUG_PTROBJ
  llvm::errs() << "set ptr " << Object << " for instruction " << *I << "\n";
#endif
}

PointerTarget TracedFunction::getPointerObject(llvm::Value const *V) const
{
  if (auto const I = llvm::dyn_cast<llvm::Instruction>(V))
    return getPointerObject(I);
  else if (auto const A = llvm::dyn_cast<llvm::Argument>(V))
    return getPointerObject(A);
  else if (auto const CE = llvm::dyn_cast<llvm::ConstantExpr>(V)) {
    if (CE->isCast()) {
      return getPointerObject(CE->getOperand(0));
    }
    else if (CE->isGEPWithNoNotionalOverIndexing()) {
      return getPointerObject(CE->getOperand(0));
    }
  }
  return ThreadListener.getProcessListener().getPointerObject(V);
}

PointerTarget TracedFunction::transferPointerObject(llvm::Value const *From,
                                                    llvm::Instruction const *To)
{
  auto const Object = getPointerObject(From);
  if (Object)
    setPointerObject(To, Object);
  return Object;
}

PointerTarget
TracedFunction::transferArgPointerObjectToCall(unsigned const ArgNo)
{
  auto const Call = llvm::dyn_cast<llvm::CallInst>(ActiveInstruction);
  assert(Call && "No CallInst active!");

  return transferPointerObject(Call->getArgOperand(ArgNo), Call);
}

//===----------------------------------------------------------------------===//
// Mutators.
//===----------------------------------------------------------------------===//

void TracedFunction::addAlloca(TracedAlloca Alloca) {
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  auto const &Area = Alloca.area();
  
  if (Area.address() < StackLow || !StackLow)
    StackLow = Area.address();
  
  if (Area.lastAddress() > StackHigh || !StackHigh)
    StackHigh = Area.lastAddress();
  
  Allocas.push_back(std::move(Alloca));
}

void TracedFunction::stackSave(uintptr_t Key) {
  std::lock_guard<std::mutex> Lock(StackMutex);
    
  StackSaves[Key] = Allocas;
}

void TracedFunction::stackRestore(uintptr_t Key,
                                  TraceMemoryState &TraceMemory)
{
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  auto const &RestoreAllocas = StackSaves[Key];
  
  // Skip all matching allocas (those that are still valid after stackrestore).
  std::size_t MismatchIdx = 0;
  for (; MismatchIdx < Allocas.size(); ++MismatchIdx) {
    if (MismatchIdx >= RestoreAllocas.size()
        || Allocas[MismatchIdx] != RestoreAllocas[MismatchIdx])
    {
      break;
    }
  }

  // Remove all cleared allocas from memory.
  for (auto i = MismatchIdx; i < Allocas.size(); ++i) {
    if (Allocas[i].area().length() > 0) {
      TraceMemory.removeAllocation(Allocas[i].address());
    }
  }
  
  // Add allocations for all restored allocas.
  for (auto i = MismatchIdx; i < RestoreAllocas.size(); ++i) {
    if (RestoreAllocas[i].area().length() > 0) {
      TraceMemory.addAllocation(RestoreAllocas[i].address(),
                                RestoreAllocas[i].area().length());
    }
  }

  // Restore saved allocas.
  Allocas = RestoreAllocas;
}


} // namespace trace (in seec)

} // namespace seec
