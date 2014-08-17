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


//===----------------------------------------------------------------------===//
/// \name Thread movement.
/// @{

/// \brief Move Thread's state to the next logical thread time.
/// \return true iff the state was moved.
///
bool moveForward(ThreadState &Thread);

/// \brief Move Thread's state forward to the end of the trace.
/// \return true iff the state was moved.
///
bool moveForwardToEnd(ThreadState &Thread);

/// \brief Move Thread's state forward until the next time that a top-level Stmt
///        is completed.
/// \return true iff the state was moved.
///
bool moveForwardToCompleteTopLevelStmt(ThreadState &Thread);

/// \brief Move Thread's state to the previous logical thread time.
/// \return true iff the state was moved.
///
bool moveBackward(ThreadState &Thread);

/// \brief Move Thread's state backward to the end of the trace.
/// \return true iff the state was moved.
///
bool moveBackwardToEnd(ThreadState &Thread);

/// \brief Move Thread's state backward until the most recent preceding time
///        that a top-level Stmt was completed.
/// \return true iff the state was moved.
///
bool moveBackwardToCompleteTopLevelStmt(ThreadState &Thread);

/// @} (Thread movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for functions.
/// @{

/// \brief Move backwards to the end of the Function's entry.
/// \return true iff the state was moved.
///
bool moveToFunctionEntry(FunctionState &Function);

/// \brief Move forwards until a Function is finished.
/// \return true iff the state was moved.
///
bool moveToFunctionFinished(FunctionState &Function);

/// @} (Contextual movement for functions.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for memory.
/// @{

/// \brief Move backwards to the initial allocation of the memory that owns
///        \c Address.
/// \return true iff the state was moved.
///
bool moveToAllocation(ProcessState &Process, uintptr_t const Address);

/// \brief Move forwards until the memory that owns \c Address is deallocated.
/// \return true iff the state was moved.
///
bool moveToDeallocation(ProcessState &Process, uintptr_t const Address);

/// \brief Move State forward until the memory state in Area changes.
/// \return true iff the State was moved.
///
bool moveForwardUntilMemoryChanges(ProcessState &State, MemoryArea const &Area);

/// \brief Move State backward until the memory state in Area changes.
/// \return true iff the State was moved.
///
bool
moveBackwardUntilMemoryChanges(ProcessState &State, MemoryArea const &Area);

/// @} (Contextual movement for memory.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for FILE streams.
/// @{

/// \brief Move \c State to the write to \c Stream that produced the character
///        at \c Position.
/// \return true iff the State was moved.
bool
moveBackwardToStreamWriteAt(ProcessState &State,
                            StreamState const &Stream,
                            std::size_t const Position);

/// @} (Contextual movement for FILE streams.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement based on AST nodes.
/// @{

/// \brief Move State forward until the Stmt is next evaluated.
///
bool moveForwardUntilEvaluated(ThreadState &State, clang::Stmt const *S);

/// \brief Move State backward until the Stmt is last evaluated.
///
bool moveBackwardUntilEvaluated(ThreadState &State, clang::Stmt const *S);

/// @} (Contextual movement based on AST nodes.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDSTATEMOVEMENT_HPP
