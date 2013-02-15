//===- lib/Clang/MappedStateMovement.cpp ----------------------------------===//
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

#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/StateMovement.hpp"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// Thread movement.

bool moveForward(ThreadState &Thread) {
  return seec::trace::moveForward(Thread.getUnmappedState());
}

bool moveBackward(ThreadState &Thread) {
  return seec::trace::moveBackward(Thread.getUnmappedState());
}

// (Thread movement.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec
