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
  
public:
  /// \name Constructors
  /// @{
  
  /// \brief Default constructor.
  ThreadedStateMovementHelper()
  : ProcessStateMutex(),
    ProcessStateCV()
  {}
  
  ThreadedStateMovementHelper(ThreadedStateMovementHelper const &) = delete;
  
  ThreadedStateMovementHelper(ThreadedStateMovementHelper &&) = delete;
  
  /// @}
  
  void addNextEventBlock(ThreadState &State) {
    auto const LastEvent = State.getTrace().events().end();
    
    std::unique_lock<std::mutex> UpdateLock(ProcessStateMutex, std::defer_lock);

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

      if (NextEvent->isBlockStart())
        break;
    }
  }

  void removePreviousEventBlock(ThreadState &State) {
    auto const FirstEvent = State.getTrace().events().begin();
    
    std::unique_lock<std::mutex> UpdateLock(ProcessStateMutex, std::defer_lock);

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
};

/// \brief Move State in Direction until ProcessPredicate is true.
/// \return true iff State was moved.
bool move(ProcessState &State,
          std::function<bool (ProcessState &)> ProcessPredicate) {
  std::vector<std::thread> ThreadWorkers;
  
  for (auto &ThreadStatePtr : State.getThreadStates()) {
    // Create a new worker thread to move this ThreadState.
    ThreadWorkers.emplace_back([=](){
      // Thread worker code...
    });
  }
  
  // Wait for all thread workers to complete.
  for (auto &Worker : ThreadWorkers) {
    Worker.join();
  }
  
  return false;
}


//===------------------------------------------------------------------------===
// ProcessState movement
//===------------------------------------------------------------------------===

bool moveForwardUntil(ProcessState &State,
                      std::function<bool (ProcessState &)> Predicate) {
  return false;
}

bool moveBackwardUntil(ProcessState &State,
                       std::function<bool (ProcessState &)> Predicate) {
  return false;
}

bool moveForward(ProcessState &State) {
  auto PreviousTime = State.getProcessTime();
  ++State;
  return PreviousTime != State.getProcessTime();
}

bool moveBackward(ProcessState &State) {
  auto PreviousTime = State.getProcessTime();
  --State;
  return PreviousTime != State.getProcessTime();
}

bool moveToTime(ProcessState &State, uint64_t ProcessTime) {
  auto PreviousTime = State.getProcessTime();
  State.setProcessTime(ProcessTime);
  return PreviousTime != State.getProcessTime();
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
