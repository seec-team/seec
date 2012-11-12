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

enum class MovementType {
  None,
  Forward,
  Backward
};

class ThreadWorkerCoordinator {

};

/// \brief Move State in Direction until ProcessPredicate is true.
/// \return true iff State was moved.
bool move(ProcessState &State,
          MovementType Direction,
          std::function<bool (ProcessState &)> ProcessPredicate) {
  if (Direction == MovementType::None)
    return false;
  
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

void addNextEventBlock(ThreadState &State) {
  auto const LastEvent = State.getTrace().events().end();
  
  seec::util::Maybe<ProcessState::ScopedUpdate> SharedUpdate;
  seec::util::Maybe<uint64_t> NewProcessTime;

  while (true) {
    auto NextEvent = State.getNextEvent();
    if (NextEvent == LastEvent)
      break;
    
    // Make sure we have permission to update the shared state of the
    // ProcessState, if the next event is going to require it.
    if (!SharedUpdate.assigned() && NextEvent->modifiesSharedState()) {
      NewProcessTime = NextEvent->getProcessTime();
      assert(NewProcessTime.assigned());
      auto const WaitForProcessTime = NewProcessTime.get<0>() - 1;
      SharedUpdate = State.getParent().getScopedUpdate(WaitForProcessTime);
    }

    State.addNextEvent();

    if (NextEvent->isBlockStart())
      break;
  }
}

void removePreviousEventBlock(ThreadState &State) {
  auto const FirstEvent = State.getTrace().events().begin();
  
  seec::util::Maybe<ProcessState::ScopedUpdate> SharedUpdate;
  seec::util::Maybe<uint64_t> NewProcessTime;

  while (true) {
    auto PreviousEvent = State.getNextEvent();
    if (PreviousEvent == FirstEvent)
      break;
    
    --PreviousEvent;

    // Make sure we have permission to update the shared state of the
    // ProcessState, if the next event is going to require it.
    if (!SharedUpdate.assigned() && PreviousEvent->modifiesSharedState()) {
      NewProcessTime = PreviousEvent->getProcessTime();
      assert(NewProcessTime.assigned());
      SharedUpdate = State.getParent().getScopedUpdate(NewProcessTime.get<0>());
    }

    State.removePreviousEvent();

    if (PreviousEvent->isBlockStart())
      break;
  }
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
  
  bool Moved = false;

  if (State.getThreadTime() < ThreadTime) {
    // Move forward
    auto LastEvent = State.getTrace().events().end();

    while (State.getThreadTime() < ThreadTime
           && State.getNextEvent() != LastEvent) {
      addNextEventBlock(State);
      Moved = true;
    }
  }
  else {
    // Move backward
    auto FirstEvent = State.getTrace().events().begin();

    while (State.getThreadTime() > ThreadTime
           && State.getNextEvent() != FirstEvent) {
      removePreviousEventBlock(State);
      Moved = true;
    }
  }

  return Moved;
}

} // namespace trace (in seec)

} // namespace seec
