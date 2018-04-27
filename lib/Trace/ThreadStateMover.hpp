//===- lib/seec/Trace/ThreadStateMover.hpp -------------------------- C++ -===//
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

#ifndef SEEC_TRACE_THREADSTATEMOVER_HPP
#define SEEC_TRACE_THREADSTATEMOVER_HPP

namespace seec {
namespace trace {

class ThreadState;

void addNextEvent(ThreadState &State);

void removePreviousEvent(ThreadState &State);

} // namespace trace (in seec)
} // namespace seec

#endif // SEEC_TRACE_THREADSTATEMOVER_HPP
