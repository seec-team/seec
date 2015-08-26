//===- lib/Trace/StateMovement.cpp ----------------------------------------===//
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

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StateMovement.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceSearch.hpp"

#include <condition_variable>
#include <initializer_list>
#include <map>
#include <thread>
#include <vector>

namespace seec {

namespace trace {


/// \brief Implements state movement logic.
///
class ThreadedStateMovementHelper {
  /// Controls access to the ProcessState.
  std::mutex ProcessStateMutex;
  
  /// Used to wait for the ProcessState to update.
  std::condition_variable ProcessStateCV;
  
  /// Indicates that the movement has satisfied a predicate.
  std::atomic<bool> MovementComplete;
  
public:
  /// \name Constructors
  /// @{
  
  /// \brief Default constructor.
  ThreadedStateMovementHelper()
  : ProcessStateMutex(),
    ProcessStateCV(),
    MovementComplete(false)
  {}
  
  ThreadedStateMovementHelper(ThreadedStateMovementHelper const &) = delete;
  
  ThreadedStateMovementHelper(ThreadedStateMovementHelper &&) = delete;
  
  /// @}
  
  bool addNextEventBlock(ThreadState &State,
                         std::unique_lock<std::mutex> &UpdateLock) {
    auto const LastEvent = State.getTrace().events().end();
    auto const RewindNextEvent = State.getNextEvent();
    if (RewindNextEvent == LastEvent)
      return false;
    
    auto NextEvent = RewindNextEvent;

    while (true) {
      // Wait until the ProcessState reaches the appropriate process time
      // before we continue adding events.
      // If the event is just updating the local process time, then we must
      // wait until the ProcessState has reached this time. If the event will
      // set the ProcessState's time, then we must wait until the ProcessState
      // is at the time immediately prior to this new time.
      if (!UpdateLock) {
        auto MaybeNewProcessTime = NextEvent->getProcessTime();
        if (MaybeNewProcessTime.assigned()) {
          auto const &ProcState = State.getParent();
          auto const WaitUntil = NextEvent->modifiesSharedState()
                               ? MaybeNewProcessTime.get<0>() - 1
                               : MaybeNewProcessTime.get<0>();
          
          UpdateLock.lock();
          ProcessStateCV.wait(UpdateLock,
                              [=, &ProcState](){
                                return MovementComplete ||
                                       ProcState.getProcessTime() >= WaitUntil;
                              });
          
          if (MovementComplete) {
            // We can release the lock, because we'll only be rewinding local
            // changes. (If there were non-local changes in our rewinding area,
            // then we would already have owned the lock from applying them,
            // and thus would not be in this branch)
            UpdateLock.unlock();
            
            // Rewind changes.
            while (State.getNextEvent() != RewindNextEvent)
              State.removePreviousEvent();
            
            return false;
          }
        }
      }
      
      State.addNextEvent();
      
      NextEvent = State.getNextEvent();
      if (NextEvent->isBlockStart() || NextEvent == LastEvent)
        return true;
    }
  }
  
  bool addNextEventBlock(ThreadState &State) {
    std::unique_lock<std::mutex> UpdateLock(ProcessStateMutex, std::defer_lock);
    return addNextEventBlock(State, UpdateLock);
  }

  bool removePreviousEventBlock(ThreadState &State,
                                std::unique_lock<std::mutex> &UpdateLock) {
    auto const FirstEvent = State.getTrace().events().begin();
    auto const RewindNextEvent = State.getNextEvent();
    if (RewindNextEvent == FirstEvent)
      return false;
    
    auto PreviousEvent = RewindNextEvent;

    while (true) {
      --PreviousEvent;

      // Wait until the ProcessState reaches the appropriate process time
      // before we continue removing events.
      // If the event just updates the local process time, then we must wait
      // until the ProcessState has reached an earlier time. If the event will
      // set the ProcessState's time, then we must wait until the ProcessState
      // is at the time equal to the event's time (the removal of the event
      // will then cause the ProcessState to go to the next earliest time).
      if (!UpdateLock) {
        auto MaybeNewProcessTime = PreviousEvent->getProcessTime();
        if (MaybeNewProcessTime.assigned()) {
          auto const &ProcState = State.getParent();
          auto const WaitUntil = PreviousEvent->modifiesSharedState()
                               ? MaybeNewProcessTime.get<0>()
                               : MaybeNewProcessTime.get<0>() - 1;
          
          UpdateLock.lock();
          ProcessStateCV.wait(UpdateLock,
                              [=, &ProcState](){
                                return MovementComplete ||
                                       ProcState.getProcessTime() <= WaitUntil;
                              });
          
          if (MovementComplete) {
            // We can release the lock, because we'll only be rewinding local
            // changes. (If there were non-local changes in our rewinding area,
            // then we would already have owned the lock from applying them,
            // and thus would not be in this branch)
            UpdateLock.unlock();
            
            // Rewind changes.
            while (State.getNextEvent() != RewindNextEvent)
              State.addNextEvent();
            
            return false;
          }
        }
      }
      
      State.removePreviousEvent();
      
      if (PreviousEvent->isBlockStart() || PreviousEvent == FirstEvent)
        return true;
      
      PreviousEvent = State.getNextEvent();
    }
  }
  
  bool removePreviousEventBlock(ThreadState &State) {
    std::unique_lock<std::mutex> UpdateLock(ProcessStateMutex, std::defer_lock);
    return removePreviousEventBlock(State, UpdateLock);
  }
  
  MovementResult moveForward(ProcessState &State,
                             ProcessPredTy ProcessPredicate,
                             ThreadPredMapTy ThreadPredicates)
  {
    std::atomic<bool> Moved(false);
    std::atomic<bool> PredicateWasSatisfied(false);
    std::vector<std::thread> Workers;
    
    for (auto &ThreadStatePtr : State.getThreadStates()) {
      auto const RawPtr = ThreadStatePtr.get();
      ThreadPredTy ThreadPred;
      
      // Get the thread-specific predicate, if one exists.
      auto const ThreadPredIt = ThreadPredicates.find(RawPtr);
      if (ThreadPredIt != ThreadPredicates.end())
        ThreadPred = ThreadPredIt->second;
      
      // Create a new worker thread to move this ThreadState.
      Workers.emplace_back(
      [=, &State, &ProcessPredicate, &Moved, &PredicateWasSatisfied]()
      {
        // Thread worker code
        auto const LastEvent = RawPtr->getTrace().events().end();
        
        while (RawPtr->getNextEvent() != LastEvent) {
          // Add the next event block from this thread.
          std::unique_lock<std::mutex> Lock(ProcessStateMutex, std::defer_lock);
          if (addNextEventBlock(*RawPtr, Lock))
            Moved = true;
          
          // Check if another worker satisfied the movement.
          if (MovementComplete)
            break;
          
          // If the event acquired the shared process lock, then it might have
          // updated the ProcessState, in which case check the ProcessPredicate.
          if (Lock && ProcessPredicate && ProcessPredicate(State)) {
            MovementComplete = true;
            PredicateWasSatisfied = true;
            ProcessStateCV.notify_all();
            break;
          }
          
          // Check the thread-specific predicate, if one exists.
          if (ThreadPred && ThreadPred(*RawPtr)) {
            MovementComplete = true;
            PredicateWasSatisfied = true;
            ProcessStateCV.notify_all();
            break;
          }
        }
      });
    }
    
    // Wait for all thread workers to complete.
    for (auto &Worker : Workers) {
      Worker.join();
    }
    
    if (PredicateWasSatisfied)
      return MovementResult::PredicateSatisfied;
    else if (Moved)
      return MovementResult::ReachedEnd;
    else
      return MovementResult::Unmoved;
  }
  
  MovementResult moveBackward(ProcessState &State,
                              ProcessPredTy ProcessPredicate,
                              ThreadPredMapTy ThreadPredicates)
  {
    std::atomic<bool> Moved(false);
    std::atomic<bool> PredicateWasSatisfied(false);
    std::vector<std::thread> Workers;
    
    for (auto &ThreadStatePtr : State.getThreadStates()) {
      auto RawPtr = ThreadStatePtr.get();
      ThreadPredTy ThreadPred;
      
      // Get the thread-specific predicate, if one exists.
      auto const ThreadPredIt = ThreadPredicates.find(RawPtr);
      if (ThreadPredIt != ThreadPredicates.end())
        ThreadPred = ThreadPredIt->second;
      
      // Create a new worker thread to move this ThreadState.
      Workers.emplace_back(
      [=, &State, &ProcessPredicate, &Moved, &PredicateWasSatisfied]()
      {
        // Thread worker code
        auto const FirstEvent = RawPtr->getTrace().events().begin();
        
        while (RawPtr->getNextEvent() != FirstEvent) {
          // Add the next event block from this thread.
          std::unique_lock<std::mutex> Lock(ProcessStateMutex, std::defer_lock);
          if (removePreviousEventBlock(*RawPtr, Lock))
            Moved = true;
          
          // Check if another worker satisfied the movement.
          if (MovementComplete)
            break;
          
          // If the event acquired the shared process lock, then it might have
          // updated the ProcessState, in which case check the ProcessPredicate.
          if (Lock && ProcessPredicate && ProcessPredicate(State)) {
            MovementComplete = true;
            PredicateWasSatisfied = true;
            ProcessStateCV.notify_all();
            break;
          }
          
          // Check the thread-specific predicate, if one exists.
          if (ThreadPred && ThreadPred(*RawPtr)) {
            MovementComplete = true;
            PredicateWasSatisfied = true;
            ProcessStateCV.notify_all();
            break;
          }
        }
      });
    }
    
    // Wait for all thread workers to complete.
    for (auto &Worker : Workers) {
      Worker.join();
    }
    
    if (PredicateWasSatisfied)
      return MovementResult::PredicateSatisfied;
    else if (Moved)
      return MovementResult::ReachedBeginning;
    else
      return MovementResult::Unmoved;
  }
};


//===------------------------------------------------------------------------===
// ProcessState movement
//===------------------------------------------------------------------------===

MovementResult moveForwardUntil(ProcessState &State, ProcessPredTy Predicate)
{
  ThreadedStateMovementHelper Mover;
  return Mover.moveForward(State, Predicate, ThreadPredMapTy{});
}

MovementResult moveBackwardUntil(ProcessState &State, ProcessPredTy Predicate)
{
  ThreadedStateMovementHelper Mover;
  return Mover.moveBackward(State, Predicate, ThreadPredMapTy{});
}

MovementResult moveForward(ProcessState &State)
{
  auto const ProcessTime = State.getProcessTime();
  
  if (ProcessTime == State.getTrace().getFinalProcessTime())
    return MovementResult::Unmoved;
  
  return moveForwardUntil(State,
                          [=](ProcessState &NewState) -> bool {
                            return NewState.getProcessTime() > ProcessTime;
                          });
}

MovementResult moveBackward(ProcessState &State)
{
  auto const ProcessTime = State.getProcessTime();
  
  if (ProcessTime == 0)
    return MovementResult::Unmoved;
  
  return moveBackwardUntil(State,
                           [=](ProcessState &NewState){
                             return NewState.getProcessTime() < ProcessTime;
                           });
}

MovementResult moveForwardUntilMemoryChanges(ProcessState &State,
                                             MemoryArea const &Area)
{
  auto const Region = State.getMemory().getRegion(Area);
  auto const CurrentInit = Region.getByteInitialization();
  auto const CurrentData = Region.getByteValues();

  std::vector<unsigned char> Init(CurrentInit.begin(), CurrentInit.end());
  std::vector<unsigned char> Data(CurrentData.begin(), CurrentData.end());

  // This takes advantage of the fact that movement modifies the ProcessState
  // in-place (hence the Region is still valid).
  return moveForwardUntil(State,
          [&] (ProcessState &) -> bool {
            if (!Region.isAllocated())
              return true;

            auto const NewInit = Region.getByteInitialization();
            auto const NewData = Region.getByteValues();

            for (std::size_t i = 0; i < Init.size(); ++i)
              if (Init[i] != NewInit[i] ||
                  (Init[i] & Data[i]) != (NewInit[i] & NewData[i]))
                return true;

            return false;
          });
}

MovementResult moveBackwardUntilMemoryChanges(ProcessState &State,
                                              MemoryArea const &Area)
{
  auto const Region = State.getMemory().getRegion(Area);
  auto const CurrentInit = Region.getByteInitialization();
  auto const CurrentData = Region.getByteValues();

  std::vector<unsigned char> Init(CurrentInit.begin(), CurrentInit.end());
  std::vector<unsigned char> Data(CurrentData.begin(), CurrentData.end());

  // This takes advantage of the fact that movement modifies the ProcessState
  // in-place (hence the Region is still valid).
  return moveBackwardUntil(State,
          [&] (ProcessState &) -> bool {
            if (!Region.isAllocated())
              return true;

            auto const NewInit = Region.getByteInitialization();
            auto const NewData = Region.getByteValues();

            for (std::size_t i = 0; i < Init.size(); ++i)
              if (Init[i] != NewInit[i] ||
                  (Init[i] & Data[i]) != (NewInit[i] & NewData[i]))
                return true;

            return false;
          });
}

MovementResult moveBackwardToStreamWriteAt(ProcessState &State,
                                           StreamState const &Stream,
                                           std::size_t const Position)
{
  if (Position >= Stream.getWritten().size())
    return MovementResult::Unmoved;

  auto const Address = Stream.getAddress();
  auto const Write = Stream.getWriteAt(Position);

  auto const Moved = moveBackwardUntil(State,
    [=] (ProcessState const &P) -> bool {
      auto const StreamPtr = P.getStream(Address);
      return StreamPtr->getWritten().size() == Write.Begin;
    });

  if (Moved == MovementResult::PredicateSatisfied)
    moveForward(State);

  return Moved;
}

MovementResult moveBackwardUntilAllocated(ProcessState &State,
                                          stateptr_ty const Address)
{
  auto const Moved = moveBackwardUntil(State,
    [=] (ProcessState const &P) -> bool {
      return P.getStream(Address)
          || P.getDir(Address)
          || P.getContainingMemoryArea(Address).assigned<seec::MemoryArea>();
    });

  return Moved;
}


//===------------------------------------------------------------------------===
// ThreadState movement
//===------------------------------------------------------------------------===

MovementResult moveForwardUntil(ThreadState &State, ThreadPredTy Predicate)
{
  ThreadedStateMovementHelper Mover;
  return Mover.moveForward(State.getParent(),
                           ProcessPredTy{},
                           ThreadPredMapTy{std::make_pair(&State, Predicate)});
}

MovementResult moveBackwardUntil(ThreadState &State, ThreadPredTy Predicate)
{
  ThreadedStateMovementHelper Mover;
  return Mover.moveBackward(State.getParent(),
                            ProcessPredTy{},
                            ThreadPredMapTy{std::make_pair(&State, Predicate)});
}

MovementResult moveForward(ThreadState &State)
{
  return moveForwardUntil(State, [](ThreadState &){return true;});
}

MovementResult moveBackward(ThreadState &State)
{
  return moveBackwardUntil(State, [](ThreadState &){return true;});
}


//===------------------------------------------------------------------------===
// ThreadState queries
//===------------------------------------------------------------------------===

llvm::Instruction const *
getNextInstructionInActiveFunction(ThreadState const &State) {
  auto const ActiveFunction = State.getActiveFunction();
  if (!ActiveFunction)
    return nullptr;
  
  auto const &Trace = State.getTrace();
  
  // Find the next instruction event that is part of the same function as the
  // currently active event, if there is one.
  auto const MaybeRef =
    findInFunction(Trace,
                   rangeAfterIncluding(Trace.events(), State.getNextEvent()),
                   [](EventRecordBase const &Ev) {
                     return Ev.isInstruction();
                   });
  
  if (!MaybeRef.assigned<EventReference>())
    return nullptr;

  auto const MaybeIdx = MaybeRef.get<EventReference>()->getIndex();
  if (!MaybeIdx.assigned(0))
    return nullptr;
  
  return ActiveFunction->getInstruction(MaybeIdx.get<0>());
}

llvm::Instruction const *
getPreviousInstructionInActiveFunction(ThreadState const &State) {
  auto const ActiveFunction = State.getActiveFunction();
  if (!ActiveFunction || !ActiveFunction->getActiveInstruction())
    return nullptr;
  
  auto const &Trace = State.getTrace();
  
  // Find the next instruction event that is part of the same function as the
  // currently active event, if there is one.
  auto const MaybeRef = rfindInFunction(Trace,
                                        rangeBefore(Trace.events(),
                                                    --State.getNextEvent()),
                                        [](EventRecordBase const &Ev) {
                                          return Ev.isInstruction();
                                        });
  
  if (!MaybeRef.assigned<EventReference>())
    return nullptr;

  auto const MaybeIdx = MaybeRef.get<EventReference>()->getIndex();
  if (!MaybeIdx.assigned(0))
    return nullptr;

  return ActiveFunction->getInstruction(MaybeIdx.get<0>());
}

bool
findPreviousInstructionInActiveFunctionIf(ThreadState const &State,
                                          InstructionPredTy Predicate)
{
  auto const ActiveFunction = State.getActiveFunction();
  if (!ActiveFunction || !ActiveFunction->getActiveInstruction())
    return false;
  
  auto const &Trace = State.getTrace();
  
  auto const MaybeRef =
    rfindInFunction(Trace,
                    rangeBefore(Trace.events(), --State.getNextEvent()),
                    [=] (EventRecordBase const &Ev) -> bool {
                      if (!Ev.isInstruction())
                        return false;
                      
                      auto const MaybeIdx = Ev.getIndex();
                      if (!MaybeIdx.assigned(0))
                        return false;
                      
                      auto const Idx = MaybeIdx.get<0>();
                      auto const Inst = ActiveFunction->getInstruction(Idx);
                      if (!Inst)
                        return false;
                      
                      return Predicate(*Inst);
                    });
  
  return MaybeRef.assigned<EventReference>();
}


} // namespace trace (in seec)

} // namespace seec
