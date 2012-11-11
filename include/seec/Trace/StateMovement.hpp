//===- include/seec/Trace/StateMovement.hpp ------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_STATEMOVEMENT_HPP
#define SEEC_TRACE_STATEMOVEMENT_HPP

#include <functional>

namespace seec {

namespace trace {

class ProcessState;
class ThreadState;

/// \brief Move State forward until Predicate returns true.
/// \return true iff the State was moved.
bool moveForwardUntil(ProcessState &State,
                      std::function<bool (ProcessState &)> Predicate);

/// \brief Move State backward until Predicate returns true.
/// \return true iff the State was moved.
bool moveBackwardUntil(ProcessState &State,
                       std::function<bool (ProcessState &)> Predicate);

/// \brief Move State forward until Predicate returns true.
/// \return true iff the State was moved.
bool moveForwardUntil(ThreadState &State,
                      std::function<bool (ThreadState &)> Predicate);

/// \brief Move State backward until Predicate returns true.
/// \return true iff the State was moved.
bool moveBackwardUntil(ThreadState &State,
                       std::function<bool (ThreadState &)> Predicate);

/// \brief Move State forward to the next thread time.
/// \return true iff the State was moved.
bool moveForward(ThreadState &State);

/// \brief Move State backward to the previous thread time.
/// \return true iff the State was moved.
bool moveBackward(ThreadState &State);

}

}

#endif // SEEC_TRACE_STATEMOVEMENT_HPP
