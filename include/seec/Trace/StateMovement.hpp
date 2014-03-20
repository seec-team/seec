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
typedef std::function<bool (llvm::Instruction const &)> InstructionPredTy;

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

/// \brief Move State forward until the memory state in Area changes.
/// \return true iff the State was moved.
bool moveForwardUntilMemoryChanges(ProcessState &State, MemoryArea const &Area);

/// \brief Move State backward until the memory state in Area changes.
/// \return true iff the State was moved.
bool
moveBackwardUntilMemoryChanges(ProcessState &State, MemoryArea const &Area);

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

/// \brief Check if any previously executed \c llvm::Instruction in the active
///        \c FunctionState matches the given \c Predicate.
/// \param State the \c ThreadState to search.
/// \param Predicate the predicate to check each previously executed
///        \c llvm::Instruction against.
/// \return true iff \c Predicate(I) returns true for any \c llvm::Instruction
///         I that was previously executed in the active \c FunctionState.
///
bool
findPreviousInstructionInActiveFunctionIf(ThreadState const &State,
                                          InstructionPredTy Predicate);

/// @} (ThreadState queries)

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_STATEMOVEMENT_HPP
