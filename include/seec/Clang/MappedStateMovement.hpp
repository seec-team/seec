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


namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


class ThreadState;


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


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDSTATEMOVEMENT_HPP
