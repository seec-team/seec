//===- lib/Trace/TraceStreams.cpp ----------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
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
  std::lock_guard<std::mutex> Lock(StreamsMutex);
  
  Streams.insert(std::make_pair(stream, TraceStream{}));
}
  
bool TraceStreams::streamWillClose(FILE *stream) const {
  std::lock_guard<std::mutex> Lock(StreamsMutex);
  
  auto It = Streams.find(stream);
  
  return It != Streams.end();
}

void TraceStreams::streamClosed(FILE *stream) {
  std::lock_guard<std::mutex> Lock(StreamsMutex);
  
  auto It = Streams.find(stream);
  assert(It != Streams.end());
  
  Streams.erase(It);
}


} // namespace trace (in seec)

} // namespace seec
