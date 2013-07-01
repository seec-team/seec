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

namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


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

/// \brief Move Thread's state to the previous logical thread time.
/// \return true iff the state was moved.
///
bool moveBackward(ThreadState &Thread);

/// \brief Move Thread's state backward to the end of the trace.
/// \return true iff the state was moved.
///
bool moveBackwardToEnd(ThreadState &Thread);

/// @} (Thread movement.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for values.
/// @{

/// \brief Move backwards to the initial allocation of a Value.
/// \return true iff the state was moved.
///
bool moveToAllocation(ProcessState &Process, Value const &OfValue);

/// \brief Move forwards until a Value is deallocated.
/// \return true iff the state was moved.
///
bool moveToDeallocation(ProcessState &Process, Value const &OfValue);

/// @} (Contextual movement for values.)
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
/// \name Contextual movement for memory.
/// @{

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


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDSTATEMOVEMENT_HPP
