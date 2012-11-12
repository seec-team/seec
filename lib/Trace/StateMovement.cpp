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
  
  void addNextEventBlock(ThreadState &State,
                         std::unique_lock<std::mutex> &UpdateLock) {
    auto const LastEvent = State.getTrace().events().end();

    while (true) {
      auto NextEvent = State.getNextEvent();
      if (NextEvent == LastEvent)
        break;
      
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
                                return ProcState.getProcessTime() >= WaitUntil;
                              });
        }
      }

      State.addNextEvent();

      if (State.getNextEvent()->isBlockStart())
        break;
    }
  }
  
  void addNextEventBlock(ThreadState &State) {
    std::unique_lock<std::mutex> UpdateLock(ProcessStateMutex, std::defer_lock);
    addNextEventBlock(State, UpdateLock);
  }

  void removePreviousEventBlock(ThreadState &State,
                                std::unique_lock<std::mutex> &UpdateLock) {
    auto const FirstEvent = State.getTrace().events().begin();

    while (true) {
      auto PreviousEvent = State.getNextEvent();
      if (PreviousEvent == FirstEvent)
        break;
      
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
                                return ProcState.getProcessTime() <= WaitUntil;
                              });
        }
      }

      State.removePreviousEvent();

      if (PreviousEvent->isBlockStart())
        break;
    }
  }
  
  void removePreviousEventBlock(ThreadState &State) {
    std::unique_lock<std::mutex> UpdateLock(ProcessStateMutex, std::defer_lock);
    removePreviousEventBlock(State, UpdateLock);
  }
  
  bool moveForward(ProcessState &State,
                   std::function<bool (ProcessState &)> ProcessPredicate) {
    std::atomic<bool> Moved(false);
    std::vector<std::thread> Workers;
    
    for (auto &ThreadStatePtr : State.getThreadStates()) {
      auto RawPtr = ThreadStatePtr.get();
      
      // Create a new worker thread to move this ThreadState.
      Workers.emplace_back([=, &State, &ProcessPredicate, &Moved](){
        // Thread worker code...
        auto const LastEvent = RawPtr->getTrace().events().end();
        
        while (RawPtr->getNextEvent() != LastEvent) {
          // Add the next event block from this thread.
          std::unique_lock<std::mutex> Lock(ProcessStateMutex, std::defer_lock);
          addNextEventBlock(*RawPtr, Lock);
          Moved = true;
          
          // If the event acquired the shared process lock, then it might have
          // updated the ProcessState, in which case check the ProcessPredicate.
          if (Lock && ProcessPredicate(State)) {
            MovementComplete = true;
            ProcessStateCV.notify_all();
            break;
          }
          
          // TODO: thread predicates.
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
                    std::function<bool (ProcessState &)> ProcessPredicate) {
    std::atomic<bool> Moved(false);
    std::vector<std::thread> Workers;
    
    for (auto &ThreadStatePtr : State.getThreadStates()) {
      auto RawPtr = ThreadStatePtr.get();
      
      // Create a new worker thread to move this ThreadState.
      Workers.emplace_back([=, &State, &ProcessPredicate, &Moved](){
        // Thread worker code...
        auto const FirstEvent = RawPtr->getTrace().events().begin();
        
        while (RawPtr->getNextEvent() != FirstEvent) {
          // Add the next event block from this thread.
          std::unique_lock<std::mutex> Lock(ProcessStateMutex, std::defer_lock);
          removePreviousEventBlock(*RawPtr, Lock);
          Moved = true;
          
          // If the event acquired the shared process lock, then it might have
          // updated the ProcessState, in which case check the ProcessPredicate.
          if (Lock && ProcessPredicate(State)) {
            MovementComplete = true;
            ProcessStateCV.notify_all();
            break;
          }
          
          // TODO: thread predicates.
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
                      std::function<bool (ProcessState &)> Predicate) {
  ThreadedStateMovementHelper Mover;
  return Mover.moveForward(State, Predicate);
}

bool moveBackwardUntil(ProcessState &State,
                       std::function<bool (ProcessState &)> Predicate) {
  ThreadedStateMovementHelper Mover;
  return Mover.moveBackward(State, Predicate);
}

bool moveForward(ProcessState &State) {
  auto const ProcessTime = State.getProcessTime();
  
  if (ProcessTime == State.getTrace().getFinalProcessTime())
    return false;
  
  return moveForwardUntil(State,
                          [=](ProcessState &NewState){
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

bool moveToTime(ProcessState &State, uint64_t ProcessTime) {
  auto PreviousTime = State.getProcessTime();
  
  if (PreviousTime == ProcessTime)
    return false;
  else if (PreviousTime < ProcessTime)
    return moveForwardUntil(State,
                            [=](ProcessState &NewState){
                              return NewState.getProcessTime() >= ProcessTime;
                            });
  else if (PreviousTime > ProcessTime)
    return moveBackwardUntil(State,
                             [=](ProcessState &NewState){
                               return NewState.getProcessTime() <= ProcessTime;
                             });
}


//===------------------------------------------------------------------------===
// ThreadState movement
//===------------------------------------------------------------------------===

bool moveForwardUntil(ThreadState &State,
                      std::function<bool (ThreadState &)> Predicate) {
  return false;
}

bool moveBackwardUntil(ThreadState &State,
                       std::function<bool (ThreadState &)> Predicate) {
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
