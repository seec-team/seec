//===- include/seec/Trace/StateMovement.hpp ------------------------- C++ -===//
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

#ifndef SEEC_TRACE_STATEMOVEMENT_HPP
#define SEEC_TRACE_STATEMOVEMENT_HPP

#include <cstdint>
#include <functional>
#include <map>

namespace llvm {
  class Instruction;
}

namespace seec {

namespace trace {

class ProcessState;
class ThreadState;


/// \name Typedefs
/// @{

typedef std::function<bool (ProcessState &)> ProcessPredTy;
typedef std::function<bool (ThreadState &)> ThreadPredTy;
typedef std::map<ThreadState const *, ThreadPredTy> ThreadPredMapTy;

/// @}


/// \name ProcessState movement.
/// @{

/// \brief Move State forward until Predicate returns true.
/// \return true iff the State was moved.
bool moveForwardUntil(ProcessState &State,
                      ProcessPredTy Predicate);

/// \brief Move State backward until Predicate returns true.
/// \return true iff the State was moved.
bool moveBackwardUntil(ProcessState &State,
                       ProcessPredTy Predicate);

/// \brief Move State forward to the next process time.
/// \return true iff the State was moved.
bool moveForward(ProcessState &State);

/// \brief Move State backward to the previous process time.
/// \return true iff the State was moved.
bool moveBackward(ProcessState &State);

/// \brief Move State to the given process time.
/// If State cannot assume the given process time, it will be moved as close as
/// possible.
/// \return true iff the State was moved.
bool moveToTime(ProcessState &State, uint64_t ProcessTime);

/// @} (ProcessState movement)


/// \name ThreadState movement.
/// @{

/// \brief Move State forward until Predicate returns true.
/// \return true iff the State was moved.
bool moveForwardUntil(ThreadState &State,
                      ThreadPredTy Predicate);

/// \brief Move State backward until Predicate returns true.
/// \return true iff the State was moved.
bool moveBackwardUntil(ThreadState &State,
                       ThreadPredTy Predicate);

/// \brief Move State forward to the next thread time.
/// \return true iff the State was moved.
bool moveForward(ThreadState &State);

/// \brief Move State backward to the previous thread time.
/// \return true iff the State was moved.
bool moveBackward(ThreadState &State);

/// \brief Move State to the given thread time.
/// If State cannot assume the given thread time, it will be moved as close as
/// possible.
/// \return true iff the State was moved.
bool moveToTime(ThreadState &State, uint64_t ThreadTime);

/// @} (ThreadState movement)


/// \name ThreadState queries.
/// @{

/// \brief Find the Instruction that will be active if State is moved forward.
/// \return the Instruction if it exists, or nullptr.
///
llvm::Instruction const *
getNextInstructionInActiveFunction(ThreadState const &State);

/// \brief Find the Instruction that will be active if State is moved backward.
/// \return the Instruction if it exists, or nullptr.
///
llvm::Instruction const *
getPreviousInstructionInActiveFunction(ThreadState const &State);

/// @} (ThreadState queries)

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_STATEMOVEMENT_HPP
