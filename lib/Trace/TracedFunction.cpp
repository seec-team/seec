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
#include "seec/Trace/TraceThreadListener.hpp"


namespace seec {

namespace trace {


//===----------------------------------------------------------------------===//
// Support getCurrentRuntimeValue.
//===----------------------------------------------------------------------===//

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
  assert(!EventOffsetEnd && "Function has finished recording!");
  
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  ByValArgs.emplace_back(Arg, std::move(Area));
}

seec::Maybe<seec::MemoryArea>
TracedFunction::getParamByValArea(llvm::Argument const *Arg) const
{
  assert(!EventOffsetEnd && "Function has finished recording!");

  std::lock_guard<std::mutex> Lock(StackMutex);

  for (auto const &PBV : ByValArgs)
    if (PBV.getArgument() == Arg)
      return PBV.getArea();

  return seec::Maybe<seec::MemoryArea>();
}


//===----------------------------------------------------------------------===//
// Mutators.
//===----------------------------------------------------------------------===//

void TracedFunction::finishRecording(offset_uint EventOffsetEnd,
                                     uint64_t ThreadTimeExited) {
  assert(!this->EventOffsetEnd && "Function has finished recording!");
  
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  this->EventOffsetEnd = EventOffsetEnd;
  this->ThreadTimeExited = ThreadTimeExited;
  
  // clear active-only information
  Allocas.clear();
  StackLow = 0;
  StackHigh = 0;
  CurrentValues.clear();
}

void TracedFunction::addAlloca(TracedAlloca Alloca) {
  assert(!EventOffsetEnd && "Function has finished recording!");
  
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  auto const &Area = Alloca.area();
  
  if (Area.address() < StackLow || !StackLow)
    StackLow = Area.address();
  
  if (Area.lastAddress() > StackHigh || !StackHigh)
    StackHigh = Area.lastAddress();
  
  Allocas.push_back(std::move(Alloca));
}

void TracedFunction::stackSave(uintptr_t Key) {
  assert(!EventOffsetEnd && "Function has finished recording!");
  
  std::lock_guard<std::mutex> Lock(StackMutex);
    
  StackSaves[Key] = Allocas;
}

MemoryArea TracedFunction::stackRestore(uintptr_t Key) {
  assert(!EventOffsetEnd && "Function has finished recording!");
  
  std::lock_guard<std::mutex> Lock(StackMutex);
  
  auto const &RestoreAllocas = StackSaves[Key];
  
  // Calculate invalidated memory area. This is the area occupied by all
  // allocas that are currently active, but will be removed by the restore.
  uintptr_t ClearLow = 0;
  uintptr_t ClearHigh = 0;
  
  for (std::size_t i = 0; i < Allocas.size(); ++i) {
    if (i >= RestoreAllocas.size() || Allocas[i] != RestoreAllocas[i]) {
      auto FirstArea = Allocas[i].area();
      auto FinalArea = Allocas.back().area();
      
      ClearLow = std::min(FirstArea.address(), FinalArea.address());
      ClearHigh = std::max(FirstArea.lastAddress(), FinalArea.lastAddress());
      
      break;
    }
  }
  
  // Restore saved allocas.
  Allocas = RestoreAllocas;
  
  return MemoryArea(ClearLow, (ClearHigh - ClearLow) + 1);
}


} // namespace trace (in seec)

} // namespace seec
