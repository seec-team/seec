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
#include "seec/Clang/MappedStmt.hpp"
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

static bool isLogicalPoint(seec::trace::ThreadState const &Thread,
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

bool moveToFunctionEntry(FunctionState &Function) {
  auto &UnmappedFunction = Function.getUnmappedState();
  auto &UnmappedThread = UnmappedFunction.getParent();
  auto &CallStack = UnmappedThread.getCallStack();
  
  auto const FunIt =
    std::find_if(CallStack.begin(), CallStack.end(),
                  [&] (std::unique_ptr<seec::trace::FunctionState> const &F) {
                    return F.get() == &UnmappedFunction;
                  });
  assert(FunIt != CallStack.end() && "Function is not in parent's stack.");
  
  std::size_t const StackPos = std::distance(CallStack.begin(), FunIt);
  
  // Rewind until there's no active instruction in the selected Function.
  seec::trace::moveBackwardUntil(UnmappedThread,
    [=, &UnmappedFunction] (seec::trace::ThreadState const &T) -> bool {
      auto const StackSize = T.getCallStack().size();
      
      if (StackSize < StackPos + 1)
        return true;
      else if (StackSize > StackPos + 1)
        return false;
      
      auto const ActiveFn = T.getActiveFunction();
      if (ActiveFn != &UnmappedFunction)
        return true;
      
      return (ActiveFn->getActiveInstruction() == nullptr);
    });
  
  // Move forwards until we're at a logical point.
  return moveForward(Function.getParent());
}

bool moveToFunctionFinished(FunctionState &Function) {
  auto &Thread = Function.getParent();
  auto &Process = Thread.getParent();
  auto const &MappedModule = Process.getProcessTrace().getMapping();
  
  auto &UnmappedThread = Thread.getUnmappedState();
  auto const &UnmappedFunction = Function.getUnmappedState();
  auto const &CallStack = UnmappedThread.getCallStack();
  
  auto const FunIt =
    std::find_if(CallStack.begin(), CallStack.end(),
                  [&] (std::unique_ptr<seec::trace::FunctionState> const &F) {
                    return F.get() == &UnmappedFunction;
                  });
  assert(FunIt != CallStack.end() && "Function is not in parent's stack.");
  
  std::size_t const StackPos = std::distance(CallStack.begin(), FunIt);
  
  auto const Moved = seec::trace::moveForwardUntil(UnmappedThread,
                        [=, &MappedModule] (seec::trace::ThreadState const &T) {
                          return T.getCallStack().size() <= StackPos
                                  && isLogicalPoint(T, MappedModule);
                        });
  
  Process.cacheClear();
  
  return Moved;
}

// (Contextual movement for functions.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Contextual movement for memory.

bool moveToAllocation(ProcessState &Process, uintptr_t const Address)
{
  auto &Unmapped = Process.getUnmappedProcessState();
  
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

bool moveToDeallocation(ProcessState &Process, uintptr_t const Address)
{
  auto &Unmapped = Process.getUnmappedProcessState();
  
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


//===----------------------------------------------------------------------===//
// \name Contextual movement for FILE streams.

bool
moveBackwardToStreamWriteAt(ProcessState &MappedState,
                            StreamState const &MappedStream,
                            std::size_t const Position)
{
  auto &State = MappedState.getUnmappedProcessState();
  auto &Stream = MappedStream.getUnmappedState();

  auto const Moved =
    seec::trace::moveBackwardToStreamWriteAt(State, Stream, Position);

  MappedState.cacheClear();

  return Moved;
}

// (Contextual movement for FILE streams.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// \name Contextual movement based on AST nodes.

bool moveForwardUntilEvaluated(ThreadState &Thread, clang::Stmt const *S)
{
  using namespace seec::trace;
  
  // This movement passes through three distinct states. We begin at PreStmt,
  // and continue moving forward until the active Instruction is "in" the
  // requested Stmt (S) -- it is mapped to either S or one of S's children. At
  // that point we move to StmtPartial. We continue moving forward until the
  // next Instruction that would be active (but is not yet) is not "in" S (it
  // is neither S nor any of S's children), at which point we move to
  // StmtComplete. In StmtComplete we move forward until we are at a logical
  // point to stop.
  enum class MoveStateEn {
    PreStmt,
    StmtPartial,
    StmtComplete
  };
  
  if (!S)
    return false;
  
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();
  auto const StmtChildren = seec::seec_clang::getAllChildren(S);
  
  MoveStateEn MoveState = MoveStateEn::PreStmt;
  seec::trace::FunctionState const *InFunction = nullptr;
  
  auto const Moved =
    trace::moveForwardUntil(Unmapped,
      [&] (seec::trace::ThreadState const &T) {
        auto const ActiveFn = T.getActiveFunction();
        
        if (MoveState == MoveStateEn::PreStmt && ActiveFn) {
          if (auto const ActiveInst = ActiveFn->getActiveInstruction()) {
            if (auto const ActiveStmt = MappedModule.getStmt(ActiveInst)) {
              if (S == ActiveStmt || StmtChildren.count(ActiveStmt)) {
                MoveState = MoveStateEn::StmtPartial;
                InFunction = ActiveFn;
              }
            }
          }
        }
        
        if (MoveState == MoveStateEn::StmtPartial && ActiveFn == InFunction) {
          if (auto const Next = getNextInstructionInActiveFunction(T)) {
            if (auto const NextStmt = MappedModule.getStmt(Next)) {
              if (S != NextStmt && !StmtChildren.count(NextStmt)) {
                MoveState = MoveStateEn::StmtComplete;
              }
            }
          }
          else { 
            // There are no more instructions in this function call, so the
            // Stmt must have completed.
            MoveState = MoveStateEn::StmtComplete;
          }
        }
        
        if (MoveState == MoveStateEn::StmtComplete)
          return isLogicalPoint(T, MappedModule);
        
        return false;
      });
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

bool moveBackwardUntilEvaluated(ThreadState &Thread, clang::Stmt const *S)
{
  using namespace seec::trace;
  
  enum class MoveStateEn {
    InitiallyInStmt,
    OutsideOfStmt
  };
  
  if (!S)
    return false;
  
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();
  auto const StmtChildren = seec::seec_clang::getAllChildren(S);
  
  MoveStateEn MoveState = MoveStateEn::OutsideOfStmt;
  
  // Check if we are starting movement from "within" the Stmt (in which case
  // we must rewind to the previous execution of the Stmt).
  if (auto const ActiveFn = Unmapped.getActiveFunction())
    if (auto const ActiveInst = ActiveFn->getActiveInstruction())
      if (auto const ActiveStmt = MappedModule.getStmt(ActiveInst))
        if (S == ActiveStmt || StmtChildren.count(ActiveStmt))
          MoveState = MoveStateEn::InitiallyInStmt;
  
  // First move backward until we are "within" the Stmt S (if we started in S,
  // then move until we are within the previous execution).
  auto const Moved =
    trace::moveBackwardUntil(Unmapped,
      [&] (seec::trace::ThreadState const &T) {
        if (auto const ActiveFn = T.getActiveFunction()) {
          if (auto const ActiveInst = ActiveFn->getActiveInstruction()) {
            if (auto const ActiveStmt = MappedModule.getStmt(ActiveInst)) {
              if (S == ActiveStmt || StmtChildren.count(ActiveStmt)) {
                if (MoveState == MoveStateEn::OutsideOfStmt) {
                  return true;
                }
              }
              else if (MoveState == MoveStateEn::InitiallyInStmt) {
                MoveState = MoveStateEn::OutsideOfStmt;
              }
            }
          }
        }
        
        return false;
      });
  
  // If we have to, move forward until we're at a logical point.
  if (Moved && !isLogicalPoint(Unmapped, MappedModule)) {
    trace::moveForwardUntil(Unmapped,
      [&] (seec::trace::ThreadState const &T) {
        return isLogicalPoint(T, MappedModule);
      });
  }
  
  Thread.getParent().cacheClear();
  
  return Moved;
}

// (Contextual movement based on AST nodes.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec
