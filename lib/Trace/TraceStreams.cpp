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
  llvm::outs() << "\nStream opened.\n";
}
  
void TraceStreams::streamWillClose(FILE *stream) {
  std::lock_guard<std::mutex> Lock(StreamsMutex);
  llvm::outs() << "\nStream will close.\n";
}

void TraceStreams::streamClosed(FILE *stream) {
  std::lock_guard<std::mutex> Lock(StreamsMutex);
  llvm::outs() << "\nStream closed.\n";
}


} // namespace trace (in seec)

} // namespace seec
