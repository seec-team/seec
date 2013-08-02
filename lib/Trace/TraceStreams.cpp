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

void TraceStreams::streamOpened(FILE *Stream,
                                offset_uint const FilenameOffset,
                                offset_uint const ModeOffset)
{
  Streams.insert(std::make_pair(Stream,
                                TraceStream{FilenameOffset, ModeOffset}));
}
  
bool TraceStreams::streamWillClose(FILE *Stream) const
{
  auto It = Streams.find(Stream);
  
  return It != Streams.end();
}

TraceStream const *TraceStreams::streamInfo(FILE *Stream) const
{
  auto const It = Streams.find(Stream);
  
  return (It != Streams.end()) ? &It->second : nullptr;
}

void TraceStreams::streamClosed(FILE *Stream)
{
  auto It = Streams.find(Stream);
  assert(It != Streams.end());
  
  Streams.erase(It);
}


} // namespace trace (in seec)

} // namespace seec
