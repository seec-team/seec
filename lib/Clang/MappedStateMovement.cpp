//===- lib/Clang/MappedStateMovement.cpp ----------------------------------===//
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
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StateMovement.hpp"
#include "seec/Trace/ThreadState.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// Thread movement.

bool isLogicalPoint(seec::trace::ThreadState const &Thread,
                    seec::seec_clang::MappedModule const &Mapping)
{
  // Logical points:
  // 1) No active Function.
  // 2) End of a Function's prelude.
  // 3) Instruction with mapping where the next active Instruction either does
  //    not exist or has a different mapping.
  // 4) Non-mapped Functions are not allowed.
  // 5) System header functions are not allowed.
  
  // Handles 1).
  auto const ActiveFn = Thread.getActiveFunction();
  if (!ActiveFn)
    return true;
  
  // Handles 4).
  auto const MappedFn = Mapping.getMappedFunctionDecl(ActiveFn->getFunction());
  if (!MappedFn)
    return false;
  
  // Handles 5).
  if (MappedFn->isInSystemHeader())
    return false;
  
  auto const ActiveInst = ActiveFn->getActiveInstruction();
  if (!ActiveInst) {
    // This is only a valid point if there is no active Instruction following.
    auto const Next = trace::getNextInstructionInActiveFunction(Thread);
    return Next == nullptr;
  }
  
  if (!Mapping.isMappedToStmt(*ActiveInst)) {
    // Handle 2).
    
    // If there is an unmapped Instruction following this one, then it cannot
    // be the end of the prelude (according to our definition of the prelude).
    auto const Next = trace::getNextInstructionInActiveFunction(Thread);
    if (Next && !Mapping.isMappedToStmt(*Next))
      return false;
    
    // This point is valid iff no previously active Instruction had mapping.
    return !trace::findPreviousInstructionInActiveFunctionIf(Thread,
              [&] (llvm::Instruction const &I) -> bool {
                return Mapping.isMappedToStmt(I);
              });
  }
  
  // Handles 3).
  auto const Next = trace::getNextInstructionInActiveFunction(Thread);
  return (!Next || !Mapping.areMappedToSameStmt(*ActiveInst, *Next));
}

bool moveForward(ThreadState &Thread) {
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();
  
  auto const Moved = trace::moveForwardUntil(Unmapped,
                        [&] (seec::trace::ThreadState const &T) {
                          return isLogicalPoint(T, MappedModule);
                        });
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

bool moveForwardToEnd(ThreadState &Thread) {
  auto &Unmapped = Thread.getUnmappedState();
  
  auto const Moved = seec::trace::moveForwardUntil(Unmapped,
                        [] (seec::trace::ThreadState const &T) {
                          return T.isAtEnd();
                        });
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

bool moveBackward(ThreadState &Thread) {
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();
  
  auto const Moved = trace::moveBackwardUntil(Unmapped,
                        [&] (seec::trace::ThreadState const &T) {
                          return isLogicalPoint(T, MappedModule);
                        });
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

bool moveBackwardToEnd(ThreadState &Thread) {
  auto &Unmapped = Thread.getUnmappedState();
  
  auto const Moved = seec::trace::moveBackwardUntil(Unmapped,
                        [] (seec::trace::ThreadState const &T) {
                          return T.isAtStart();
                        });
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

// (Thread movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Contextual movement for functions.

bool moveToFunctionFinished(FunctionState &Function) {
  auto &Thread = Function.getParent();
  auto &Process = Thread.getParent();
  auto const &MappedModule = Process.getProcessTrace().getMapping();
  
  auto &UnmappedThread = Thread.getUnmappedState();
  auto const StackSize = UnmappedThread.getCallStack().size();
  
  auto const Moved = seec::trace::moveForwardUntil(UnmappedThread,
                        [=, &MappedModule] (seec::trace::ThreadState const &T) {
                          return T.getCallStack().size() < StackSize
                                  && isLogicalPoint(T, MappedModule);
                        });
  
  Process.cacheClear();
  
  return Moved;
}

// (Contextual movement for functions.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Contextual movement for values.

bool moveToAllocation(ProcessState &Process, Value const &OfValue)
{
  // The allocation of virtual register values is meaningless for now.
  if (!OfValue.isInMemory())
    return false;
  
  auto &Unmapped = Process.getUnmappedProcessState();
  auto const Address = OfValue.getAddress();
  
  // Move backwards until the area is not allocated.
  auto const Moved =
    seec::trace::moveBackwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return !P.getContainingMemoryArea(Address).assigned();
      });
  
  // Now move forwards just enough that the area is allocated.
  if (Moved && !Unmapped.getContainingMemoryArea(Address).assigned()) {
    seec::trace::moveForwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return P.getContainingMemoryArea(Address).assigned();
      });
  }
  
  Process.cacheClear();
  
  return Moved;
}

bool moveToDeallocation(ProcessState &Process, Value const &OfValue)
{
  // The allocation of virtual register values is meaningless for now.
  if (!OfValue.isInMemory())
    return false;
  
  auto &Unmapped = Process.getUnmappedProcessState();
  auto const Address = OfValue.getAddress();
  
  // Move forwards until the area is not allocated.
  auto const Moved =
    seec::trace::moveForwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return !P.getContainingMemoryArea(Address).assigned();
      });
  
  // Now move backwards just enough that the area is allocated.
  if (Moved && !Unmapped.getContainingMemoryArea(Address).assigned()) {
    seec::trace::moveBackwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return P.getContainingMemoryArea(Address).assigned();
      });
  }
  
  Process.cacheClear();
  
  return Moved;
}

// (Contextual movement for values.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Contextual movement for memory.

bool moveForwardUntilMemoryChanges(ProcessState &State, MemoryArea const &Area)
{
  auto &Unmapped = State.getUnmappedProcessState();
  auto const Moved = seec::trace::moveForwardUntilMemoryChanges(Unmapped, Area);
  
  State.cacheClear();
  
  return Moved;
}

bool moveBackwardUntilMemoryChanges(ProcessState &State, MemoryArea const &Area)
{
  auto &Unmapped = State.getUnmappedProcessState();
  auto const Moved = seec::trace::moveBackwardUntilMemoryChanges(Unmapped,
                                                                 Area);
  
  State.cacheClear();
  
  return Moved;
}

// (Contextual movement for memory.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec
