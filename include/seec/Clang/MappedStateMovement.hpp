//===- include/seec/Clang/MappedStateMovement.hpp -------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDSTATEMOVEMENT_HPP
#define SEEC_CLANG_MAPPEDSTATEMOVEMENT_HPP

#include "seec/Clang/MappedStateCommon.hpp"
#include "seec/DSA/MemoryArea.hpp"

namespace clang {
  class Stmt;
}

namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


class FunctionState;
class ProcessState;
class StreamState;
class ThreadState;
class Value;


/// \brief Enumerates possible outcomes of requesting state movement.
///
enum class MovementResult {
  Unmoved,
  PredicateSatisfied,
  ReachedBeginning,
  ReachedEnd
};


//===----------------------------------------------------------------------===//
/// \name Thread movement.
/// @{

/// \brief Move Thread's state to the next logical thread time.
///
MovementResult moveForward(ThreadState &Thread);

/// \brief Move Thread's state forward to the end of the trace.
///
MovementResult moveForwardToEnd(ThreadState &Thread);

/// \brief Move Thread's state forward until the next time that a top-level Stmt
///        is completed.
///
MovementResult moveForwardToCompleteTopLevelStmt(ThreadState &Thread);

/// \brief Move Thread's state to the previous logical thread time.
///
MovementResult moveBackward(ThreadState &Thread);

/// \brief Move Thread's state backward to the end of the trace.
///
MovementResult moveBackwardToEnd(ThreadState &Thread);

/// \brief Move Thread's state backward until the most recent preceding time
///        that a top-level Stmt was completed.
///
MovementResult moveBackwardToCompleteTopLevelStmt(ThreadState &Thread);

/// @} (Thread movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for functions.
/// @{

/// \brief Move backwards to the end of the Function's entry.
///
MovementResult moveToFunctionEntry(FunctionState &Function);

/// \brief Move forwards until a Function is finished.
///
MovementResult moveToFunctionFinished(FunctionState &Function);

/// @} (Contextual movement for functions.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Process-level movement.
/// @{

/// \brief Move forwards to the next (raw) process time.
///
MovementResult moveForward(ProcessState &Process);

/// \brief Move backwards to the next (raw) process time.
///
MovementResult moveBackward(ProcessState &Process);

/// @} (Process-level movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for memory.
/// @{

/// \brief Move backwards to the initial allocation of the memory that owns
///        \c Address.
///
MovementResult moveToAllocation(ProcessState &Process,
                                stateptr_ty const Address);

/// \brief Move forwards until the memory that owns \c Address is deallocated.
///
MovementResult moveToDeallocation(ProcessState &Process,
                                  stateptr_ty const Address);

/// \brief Move State forward until the memory state in Area changes.
///
MovementResult moveForwardUntilMemoryChanges(ProcessState &State,
                                             MemoryArea const &Area);

/// \brief Move State backward until the memory state in Area changes.
///
MovementResult moveBackwardUntilMemoryChanges(ProcessState &State,
                                              MemoryArea const &Area);

/// \brief Move \c State backward until an allocation exists for the given
///        \c Address.
///
MovementResult moveBackwardUntilAllocated(ProcessState &State,
                                          stateptr_ty const Address);

/// @} (Contextual movement for memory.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for FILE streams.
/// @{

/// \brief Move \c State to the write to \c Stream that produced the character
///        at \c Position.
///
MovementResult moveBackwardToStreamWriteAt(ProcessState &State,
                                           StreamState const &Stream,
                                           std::size_t const Position);

/// @} (Contextual movement for FILE streams.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement based on AST nodes.
/// @{

/// \brief Move State forward until the Stmt is next evaluated.
///
MovementResult moveForwardUntilEvaluated(ThreadState &State,
                                         clang::Stmt const *S);

/// \brief Move State backward until the Stmt is last evaluated.
///
MovementResult moveBackwardUntilEvaluated(ThreadState &State,
                                          clang::Stmt const *S);

/// @} (Contextual movement based on AST nodes.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDSTATEMOVEMENT_HPP
