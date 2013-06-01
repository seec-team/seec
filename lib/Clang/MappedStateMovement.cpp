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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/StateMovement.hpp"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// Thread movement.

bool moveForward(ThreadState &Thread) {
  auto const Success = seec::trace::moveForward(Thread.getUnmappedState());
  Thread.getParent().cacheClear();
  return Success;
}

bool moveForwardToEnd(ThreadState &Thread) {
  auto const Moved = moveForward(Thread);
  
  if (Moved)
    while (moveForward(Thread)) ; // Intentionally empty.
  
  return Moved;
}

bool moveBackward(ThreadState &Thread) {
  auto const Success = seec::trace::moveBackward(Thread.getUnmappedState());
  Thread.getParent().cacheClear();
  return Success;
}

bool moveBackwardToEnd(ThreadState &Thread) {
  auto const Moved = moveBackward(Thread);
  
  if (Moved)
    while (moveBackward(Thread)) ; // Intentionally empty.
  
  return Moved;
}

// (Thread movement.)
//===----------------------------------------------------------------------===//


} // namespace cm (in seec)

} // namespace seec
