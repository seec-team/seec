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


//===------------------------------------------------------------------------===
// ThreadState movement
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
  auto PreviousTime = State.getThreadTime();
  State.setThreadTime(ThreadTime);
  return PreviousTime != State.getThreadTime();
}

} // namespace trace (in seec)

} // namespace seec
