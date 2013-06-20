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

void moveToEndOfStmt(seec::trace::ThreadState &Unmapped,
                     seec::seec_clang::MappedModule const &Mapping,
                     llvm::Instruction const &Current)
{
  using namespace seec::trace;
  
  while (auto const Next = getNextInstructionInActiveFunction(Unmapped)) {
    if (!Mapping.areMappedToSameStmt(Current, *Next))
      return;
    if (!seec::trace::moveForward(Unmapped))
      return;
  }
}

bool moveForward(ThreadState &Thread) {
  // Move forward until we reach the next "logical" state.
  //
  // - If we enter a new Function, we have moved to a new "logical state", but
  //   we should not stop until the prelude is complete. We will treat the last
  //   unmapped Instruction as the end of the prelude.
  //
  // - If the active Stmt changes then we have moved to a new "logical state",
  //   but we should move to the last Instruction that is mapped to this new
  //   Stmt, so that a block belonging to a Stmt appears to execute in one step.
  //
  // - If we exit a Function then this is a new "logical" state.
  
  auto &Unmapped = Thread.getUnmappedState();
  auto const &Trace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  
  auto const InitialStackSize = Unmapped.getCallStack().size();
  auto const InitialActiveFn = Unmapped.getActiveFunction();
  
  bool Moved = false;
  
  while (seec::trace::moveForward(Unmapped)) {
    Moved = true;
    
    auto const ActiveFn = Unmapped.getActiveFunction();
    
    if (ActiveFn == InitialActiveFn) {
      auto const ActiveInst = ActiveFn->getActiveInstruction();
      if (!ActiveInst || !MappedModule.isMappedToStmt(*ActiveInst))
        continue;
      
      // We're now on a new mapped Stmt. Move forward as far as possible while
      // maintaining this as the active Stmt, and then we're done.
      moveToEndOfStmt(Unmapped, MappedModule, *ActiveInst);
    }
    else {
      auto const StackSize = Unmapped.getCallStack().size();
      
      // Function has completed, move to the end of the parent function's
      // active Stmt.
      if (StackSize < InitialStackSize) {
        if (ActiveFn) {
          auto const Call = ActiveFn->getActiveInstruction();
          if (Call) {
            moveToEndOfStmt(Unmapped, MappedModule, *Call);
          }
        }
      }
      else {
        // A new function has been entered. Continue until we finish the
        // prelude.
        while (auto Next = trace::getNextInstructionInActiveFunction(Unmapped))
          if (MappedModule.isMappedToStmt(*Next)
              || !seec::trace::moveForward(Unmapped))
            break;
      }
    }
    
    break;
  }
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

bool moveForwardToEnd(ThreadState &Thread) {
  auto const Moved = moveForward(Thread);
  
  if (Moved)
    while (moveForward(Thread)) ; // Intentionally empty.
  
  return Moved;
}

bool moveBackward(ThreadState &Thread) {
  auto const Success = seec::trace::moveBackward(Thread.getUnmappedState());
  Thread.getParent().cacheClear();
  return Success;
}

bool moveBackwardToEnd(ThreadState &Thread) {
  auto const Moved = moveBackward(Thread);
  
  if (Moved)
    while (moveBackward(Thread)) ; // Intentionally empty.
  
  return Moved;
}

// (Thread movement.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec
