//===- lib/Runtimes/Tracer/SignalHandling.hpp -----------------------------===//
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

#ifndef SEEC_LIB_RUNTIMES_TRACER_SIGNALHANDLING_HPP
#define SEEC_LIB_RUNTIMES_TRACER_SIGNALHANDLING_HPP

namespace seec {

namespace trace {

class OutputStreamAllocator;
class TraceThreadListener;

void setupSignalHandling(OutputStreamAllocator *WithOutput);

void setupThreadForSignalHandling(TraceThreadListener const &ForThread);

void teardownThreadForSignalHandling();

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_LIB_RUNTIMES_TRACER_SIGNALHANDLING_HPP
