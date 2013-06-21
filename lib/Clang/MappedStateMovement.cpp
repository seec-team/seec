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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/StateMovement.hpp"
#include "seec/Trace/ThreadState.hpp"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// Thread movement.

bool isLogicalPoint(seec::trace::ThreadState const &Thread,
                    seec::seec_clang::MappedModule const &Mapping)
{
  // Logical points:
  // 1) No active Function.
  // 1) End of a Function's prelude.
  // 2) Instruction with mapping where the next active Instruction either does
  //    not exist or has a different mapping.
  
  // Handles 1).
  auto const ActiveFn = Thread.getActiveFunction();
  if (!ActiveFn)
    return true;
  
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


} // namespace cm (in seec)

} // namespace seec
