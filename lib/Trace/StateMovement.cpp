//===- lib/Trace/StateMovement.cpp ---------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StateMovement.hpp"
#include "seec/Trace/ThreadState.hpp"

#include <condition_variable>
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
  
  bool moveForward(ProcessState &State,
                   ProcessPredTy &ProcessPredicate,
                   ThreadPredMapTy ThreadPredicates) {
    std::atomic<bool> Moved(false);
    std::vector<std::thread> Workers;
    
    for (auto &ThreadStatePtr : State.getThreadStates()) {
      auto const RawPtr = ThreadStatePtr.get();
      ThreadPredTy ThreadPred;
      
      // Get the thread-specific predicate, if one exists.
      auto const ThreadPredIt = ThreadPredicates.find(RawPtr);
      if (ThreadPredIt != ThreadPredicates.end())
        ThreadPred = ThreadPredIt->second;
      
      // Create a new worker thread to move this ThreadState.
      Workers.emplace_back([=, &State, &ProcessPredicate, &Moved](){
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
            ProcessStateCV.notify_all();
            break;
          }
          
          // Check the thread-specific predicate, if one exists.
          if (ThreadPred && ThreadPred(*RawPtr)) {
            MovementComplete = true;
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
    
    return Moved;
  }
  
  bool moveBackward(ProcessState &State,
                    ProcessPredTy &ProcessPredicate,
                    ThreadPredMapTy ThreadPredicates) {
    std::atomic<bool> Moved(false);
    std::vector<std::thread> Workers;
    
    for (auto &ThreadStatePtr : State.getThreadStates()) {
      auto RawPtr = ThreadStatePtr.get();
      ThreadPredTy ThreadPred;
      
      // Get the thread-specific predicate, if one exists.
      auto const ThreadPredIt = ThreadPredicates.find(RawPtr);
      if (ThreadPredIt != ThreadPredicates.end())
        ThreadPred = ThreadPredIt->second;
      
      // Create a new worker thread to move this ThreadState.
      Workers.emplace_back([=, &State, &ProcessPredicate, &Moved](){
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
          if (Lock && ProcessPredicate(State)) {
            MovementComplete = true;
            ProcessStateCV.notify_all();
            break;
          }
          
          // Check the thread-specific predicate, if one exists.
          if (ThreadPred && ThreadPred(*RawPtr)) {
            MovementComplete = true;
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
    
    return Moved;
  }
};


//===------------------------------------------------------------------------===
// ProcessState movement
//===------------------------------------------------------------------------===

bool moveForwardUntil(ProcessState &State,
                      ProcessPredTy Predicate) {
  ThreadedStateMovementHelper Mover;
  return Mover.moveForward(State, Predicate, ThreadPredMapTy{});
}

bool moveBackwardUntil(ProcessState &State,
                       ProcessPredTy Predicate) {
  ThreadedStateMovementHelper Mover;
  return Mover.moveBackward(State, Predicate, ThreadPredMapTy{});
}

bool moveForward(ProcessState &State) {
  auto const ProcessTime = State.getProcessTime();
  
  if (ProcessTime == State.getTrace().getFinalProcessTime())
    return false;
  
  return moveForwardUntil(State,
                          [=](ProcessState &NewState) -> bool {
                            return NewState.getProcessTime() > ProcessTime;
                          });
}

bool moveBackward(ProcessState &State) {
  auto const ProcessTime = State.getProcessTime();
  
  if (ProcessTime == 0)
    return false;
  
  return moveBackwardUntil(State,
                           [=](ProcessState &NewState){
                             return NewState.getProcessTime() < ProcessTime;
                           });
}

bool moveToTime(ProcessState &State, uint64_t const ProcessTime) {
  auto const PreviousTime = State.getProcessTime();
  
  if (PreviousTime < ProcessTime)
    return moveForwardUntil(State,
                            [=](ProcessState &NewState){
                              return NewState.getProcessTime() >= ProcessTime;
                            });
  else if (PreviousTime > ProcessTime)
    return moveBackwardUntil(State,
                             [=](ProcessState &NewState){
                               return NewState.getProcessTime() <= ProcessTime;
                             });
  
  return false;
}


//===------------------------------------------------------------------------===
// ThreadState movement
//===------------------------------------------------------------------------===

bool moveForwardUntil(ThreadState &State,
                      ThreadPredTy Predicate) {
  return false;
}

bool moveBackwardUntil(ThreadState &State,
                       ThreadPredTy Predicate) {
  return false;
}

bool moveForward(ThreadState &State) {
  return false;
}

bool moveBackward(ThreadState &State) {
  return false;
}

bool moveToTime(ThreadState &State, uint64_t ThreadTime) {
  if (State.getThreadTime() == ThreadTime)
    return false;
  
  ThreadedStateMovementHelper Mover;
  bool Moved = false;

  if (State.getThreadTime() < ThreadTime) {
    // Move forward
    auto LastEvent = State.getTrace().events().end();

    while (State.getThreadTime() < ThreadTime
           && State.getNextEvent() != LastEvent) {
      Mover.addNextEventBlock(State);
      Moved = true;
    }
  }
  else {
    // Move backward
    auto FirstEvent = State.getTrace().events().begin();

    while (State.getThreadTime() > ThreadTime
           && State.getNextEvent() != FirstEvent) {
      Mover.removePreviousEventBlock(State);
      Moved = true;
    }
  }

  return Moved;
}

} // namespace trace (in seec)

} // namespace seec
