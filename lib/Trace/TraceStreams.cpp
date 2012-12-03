//===- lib/Trace/TraceStreams.cpp -----------------------------------------===//
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

#include "seec/Trace/TraceStreams.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {


//===------------------------------------------------------------------------===
// TraceFile
//===------------------------------------------------------------------------===


//===------------------------------------------------------------------------===
// TraceStream
//===------------------------------------------------------------------------===


//===------------------------------------------------------------------------===
// TraceStreams
//===------------------------------------------------------------------------===

void TraceStreams::streamOpened(FILE *stream) {
  Streams.insert(std::make_pair(stream, TraceStream{}));
}
  
bool TraceStreams::streamWillClose(FILE *stream) const {
  auto It = Streams.find(stream);
  
  return It != Streams.end();
}

void TraceStreams::streamClosed(FILE *stream) {
  auto It = Streams.find(stream);
  assert(It != Streams.end());
  
  Streams.erase(It);
}


} // namespace trace (in seec)

} // namespace seec
