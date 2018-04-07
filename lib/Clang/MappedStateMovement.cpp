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

#include "clang/AST/Expr.h"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>


namespace seec {

namespace cm {


static MovementResult toCMResult(seec::trace::MovementResult const Result)
{
  switch (Result) {
    case trace::MovementResult::Unmoved:
      return MovementResult::Unmoved;

    case trace::MovementResult::PredicateSatisfied:
      return MovementResult::PredicateSatisfied;

    case trace::MovementResult::ReachedBeginning:
      return MovementResult::ReachedBeginning;

    case trace::MovementResult::ReachedEnd:
      return MovementResult::ReachedEnd;
  }

  llvm_unreachable("Unknown MovementResult");
  return MovementResult::Unmoved;
}


//===----------------------------------------------------------------------===//
// Thread movement.

static bool isLogicalPoint(seec::trace::ThreadState const &Thread,
                           seec::seec_clang::MappedModule const &Mapping)
{
  // Logical points:
  // 1) No active Function.
  // 2) End of a Function's prelude.
  // 3) Instruction that completes a Stmt or Decl's evaluation.
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
  return Mapping.hasCompletionMapping(*ActiveInst);
}

MovementResult moveForward(ThreadState &Thread)
{
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();
  
  auto const Moved = trace::moveForwardUntil(Unmapped,
                        [&] (seec::trace::ThreadState const &T) {
                          return isLogicalPoint(T, MappedModule);
                        });
  
  Thread.getParent().cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveForwardToEnd(ThreadState &Thread)
{
  auto &Unmapped = Thread.getUnmappedState();
  
  auto const Moved = seec::trace::moveForwardUntil(Unmapped,
                        [] (seec::trace::ThreadState const &T) {
                          return T.isAtEnd();
                        });
  
  Thread.getParent().cacheClear();
  
  return toCMResult(Moved);
}

/// \brief Check if the given \c clang::Stmt is "top-level" (does not have a
///        parent that is an Expr).
///
static
bool isTopLevel(clang::Stmt const *S, seec::seec_clang::MappedAST const &AST)
{
  if (!llvm::isa<clang::Expr>(S))
    return false;

  auto const MaybeParent = AST.getParent(S);
  if (MaybeParent.assigned<clang::Stmt const *>()) {
    auto const TheStmt = MaybeParent.get<clang::Stmt const *>();

    if (auto const Expr = llvm::dyn_cast<clang::Expr>(TheStmt)) {
      // If the parent is a top-level ParenExpr then also treat this Expr as
      // top-level, because the parent won't exist in the IR.
      if (auto const Paren = llvm::dyn_cast<clang::ParenExpr>(Expr)) {
        return isTopLevel(Paren, AST);
      }

      return false;
    }

    return true;
  }

  return true;
}

MovementResult moveForwardToCompleteTopLevelStmt(ThreadState &Thread)
{
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();

  llvm::SmallVector<clang::Stmt const *, 8> Complete;

  auto const Moved = trace::moveForwardUntil(Unmapped,
    [&] (seec::trace::ThreadState const &T) -> bool {
      if (auto const ActiveFnState = T.getActiveFunction()) {
        auto const ActiveFn = ActiveFnState->getFunction();
        auto const MappedFn = MappedModule.getMappedFunctionDecl(ActiveFn);
        if (!MappedFn)
          return false;

        if (auto const ActiveInst = ActiveFnState->getActiveInstruction()) {
          auto const &AST = MappedFn->getAST();
          if (MappedModule.getStmtCompletions(*ActiveInst, AST, Complete)) {
            auto const Success = std::any_of(Complete.begin(), Complete.end(),
                                              [&] (clang::Stmt const *S) {
                                                return isTopLevel(S, AST);
                                              });
            Complete.clear();
            return Success;
          }
        }
      }

      return false;
    });

  Thread.getParent().cacheClear();

  return toCMResult(Moved);
}

MovementResult moveBackward(ThreadState &Thread)
{
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();
  
  auto const Moved = trace::moveBackwardUntil(Unmapped,
                        [&] (seec::trace::ThreadState const &T) {
                          return isLogicalPoint(T, MappedModule);
                        });
  
  Thread.getParent().cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveBackwardToEnd(ThreadState &Thread)
{
  auto &Unmapped = Thread.getUnmappedState();
  
  auto const Moved = seec::trace::moveBackwardUntil(Unmapped,
                        [] (seec::trace::ThreadState const &T) {
                          return T.isAtStart();
                        });
  
  Thread.getParent().cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveBackwardToCompleteTopLevelStmt(ThreadState &Thread)
{
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();

  llvm::SmallVector<clang::Stmt const *, 8> Complete;

  auto const Moved = trace::moveBackwardUntil(Unmapped,
    [&] (seec::trace::ThreadState const &T) -> bool {
      if (auto const ActiveFnState = T.getActiveFunction()) {
        auto const ActiveFn = ActiveFnState->getFunction();
        auto const MappedFn = MappedModule.getMappedFunctionDecl(ActiveFn);
        if (!MappedFn)
          return false;

        if (auto const ActiveInst = ActiveFnState->getActiveInstruction()) {
          auto const &AST = MappedFn->getAST();
          if (MappedModule.getStmtCompletions(*ActiveInst, AST, Complete)) {
            auto const Success = std::any_of(Complete.begin(), Complete.end(),
                                              [&] (clang::Stmt const *S) {
                                                return isTopLevel(S, AST);
                                              });
            Complete.clear();
            return Success;
          }
        }
      }

      return false;
    });

  Thread.getParent().cacheClear();

  return toCMResult(Moved);
}

// (Thread movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Contextual movement for functions.

MovementResult moveToFunctionEntry(FunctionState &Function)
{
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

MovementResult moveToFunctionFinished(FunctionState &Function)
{
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
  
  return toCMResult(Moved);
}

// (Contextual movement for functions.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Process-level movement.
/// @{

MovementResult moveForward(ProcessState &Process)
{
  auto &Unmapped = Process.getUnmappedProcessState();
  auto const Moved = seec::trace::moveForward(Unmapped);
  Process.cacheClear();
  return toCMResult(Moved);
}

MovementResult moveBackward(ProcessState &Process)
{
  auto &Unmapped = Process.getUnmappedProcessState();
  auto const Moved = seec::trace::moveBackward(Unmapped);
  Process.cacheClear();
  return toCMResult(Moved);
}

/// @} (Process-level movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Contextual movement for memory.

MovementResult moveToAllocation(ProcessState &Process,
                                stateptr_ty const Address)
{
  auto &Unmapped = Process.getUnmappedProcessState();
  
  // Move backwards until the area is not allocated.
  auto const Moved =
    seec::trace::moveBackwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return !P.getContainingMemoryArea(Address).assigned();
      });
  
  // Now move forwards just enough that the area is allocated.
  if (Moved == trace::MovementResult::PredicateSatisfied
      && !Unmapped.getContainingMemoryArea(Address).assigned())
  {
    seec::trace::moveForwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return P.getContainingMemoryArea(Address).assigned();
      });
  }
  
  Process.cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveToDeallocation(ProcessState &Process,
                                  stateptr_ty const Address)
{
  auto &Unmapped = Process.getUnmappedProcessState();
  
  // Move forwards until the area is not allocated.
  auto const Moved =
    seec::trace::moveForwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return !P.getContainingMemoryArea(Address).assigned();
      });
  
  // Now move backwards just enough that the area is allocated.
  if (Moved == trace::MovementResult::PredicateSatisfied
      && !Unmapped.getContainingMemoryArea(Address).assigned())
  {
    seec::trace::moveBackwardUntil(Unmapped,
      [=] (seec::trace::ProcessState &P) -> bool {
        return P.getContainingMemoryArea(Address).assigned();
      });
  }
  
  Process.cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveForwardUntilMemoryChanges(ProcessState &State,
                                             MemoryArea const &Area)
{
  auto &Unmapped = State.getUnmappedProcessState();
  auto const Moved = seec::trace::moveForwardUntilMemoryChanges(Unmapped, Area);
  
  State.cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveBackwardUntilMemoryChanges(ProcessState &State,
                                              MemoryArea const &Area)
{
  auto &Unmapped = State.getUnmappedProcessState();
  auto const Moved = seec::trace::moveBackwardUntilMemoryChanges(Unmapped,
                                                                 Area);
  
  State.cacheClear();
  
  return toCMResult(Moved);
}

MovementResult moveBackwardUntilAllocated(ProcessState &State,
                                          stateptr_ty const Address)
{
  auto &Unmapped = State.getUnmappedProcessState();
  auto const Moved = seec::trace::moveBackwardUntilAllocated(Unmapped, Address);

  State.cacheClear();

  return toCMResult(Moved);
}

// (Contextual movement for memory.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// \name Contextual movement for FILE streams.

MovementResult moveBackwardToStreamWriteAt(ProcessState &MappedState,
                                           StreamState const &MappedStream,
                                           std::size_t const Position)
{
  auto &State = MappedState.getUnmappedProcessState();
  auto &Stream = MappedStream.getUnmappedState();

  auto const Moved =
    seec::trace::moveBackwardToStreamWriteAt(State, Stream, Position);

  MappedState.cacheClear();

  return toCMResult(Moved);
}

// (Contextual movement for FILE streams.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// \name Contextual movement based on AST nodes.

MovementResult moveForwardUntilEvaluated(ThreadState &Thread,
                                         clang::Stmt const *S)
{
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();

  llvm::SmallVector<clang::Stmt const *, 8> Complete;

  auto const Moved = trace::moveForwardUntil(Unmapped,
    [&] (seec::trace::ThreadState const &T) -> bool {
      if (auto const ActiveFnState = T.getActiveFunction()) {
        auto const ActiveFn = ActiveFnState->getFunction();
        auto const MappedFn = MappedModule.getMappedFunctionDecl(ActiveFn);
        if (!MappedFn)
          return false;

        if (auto const ActiveInst = ActiveFnState->getActiveInstruction()) {
          auto const &AST = MappedFn->getAST();

          if (MappedModule.getStmtCompletions(*ActiveInst, AST, Complete)) {
            auto const Success = std::find(Complete.begin(), Complete.end(), S)
                                 != Complete.end();
            Complete.clear();
            return Success;
          }
        }
      }

      return false;
    });

  Thread.getParent().cacheClear();

  return toCMResult(Moved);
}

MovementResult moveBackwardUntilEvaluated(ThreadState &Thread,
                                          clang::Stmt const *S)
{
  auto &Unmapped = Thread.getUnmappedState();
  auto const &MappedTrace = Thread.getParent().getProcessTrace();
  auto const &MappedModule = MappedTrace.getMapping();

  llvm::SmallVector<clang::Stmt const *, 8> Complete;

  auto const Moved = trace::moveBackwardUntil(Unmapped,
    [&] (seec::trace::ThreadState const &T) -> bool {
      if (auto const ActiveFnState = T.getActiveFunction()) {
        auto const ActiveFn = ActiveFnState->getFunction();
        auto const MappedFn = MappedModule.getMappedFunctionDecl(ActiveFn);
        if (!MappedFn)
          return false;

        if (auto const ActiveInst = ActiveFnState->getActiveInstruction()) {
          auto const &AST = MappedFn->getAST();

          if (MappedModule.getStmtCompletions(*ActiveInst, AST, Complete)) {
            auto const Success = std::find(Complete.begin(), Complete.end(), S)
                                 != Complete.end();
            Complete.clear();
            return Success;
          }
        }
      }

      return false;
    });

  Thread.getParent().cacheClear();

  return toCMResult(Moved);
}

// (Contextual movement based on AST nodes.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec
