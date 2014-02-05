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


//===------------------------------------------------------------------------===
// TraceDirs
//===------------------------------------------------------------------------===

void TraceDirs::DIROpened(void const * const TheDIR,
                          offset_uint const DirnameOffset)
{
  Dirs.insert(std::make_pair(reinterpret_cast<uintptr_t>(TheDIR),
                             TraceDIR{DirnameOffset}));
}

bool TraceDirs::DIRWillClose(void const * const TheDIR) const
{
  auto const It = Dirs.find(reinterpret_cast<uintptr_t>(TheDIR));
  return It != Dirs.end();
}

TraceDIR const *TraceDirs::DIRInfo(void const * const TheDIR) const
{
  auto const It = Dirs.find(reinterpret_cast<uintptr_t>(TheDIR));
  return (It != Dirs.end()) ? &It->second : nullptr;
}

void TraceDirs::DIRClosed(void const * const TheDIR)
{
  auto It = Dirs.find(reinterpret_cast<uintptr_t>(TheDIR));
  assert(It != Dirs.end());
  Dirs.erase(It);
}


} // namespace trace (in seec)

} // namespace seec
