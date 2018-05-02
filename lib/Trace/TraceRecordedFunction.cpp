//===- lib/Trace/TraceRecordedFunction.cpp --------------------------------===//
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

#include "seec/Trace/TraceRecordedFunction.hpp"

namespace seec {
namespace trace {

void RecordedFunction::setCompletion(EventWriter &Writer,
                                     offset_uint const WithEventOffsetEnd,
                                     uint64_t const WithThreadTimeExited)
{
  assert(EventOffsetEnd == 0 && ThreadTimeExited == 0);
  EventOffsetEnd = WithEventOffsetEnd;
  ThreadTimeExited = WithThreadTimeExited;
  
  auto Rewrite = Writer.rewrite(StartEventWrite,
                                Index,
                                EventOffsetStart,
                                EventOffsetEnd,
                                ThreadTimeEntered,
                                ThreadTimeExited);
  assert(Rewrite);
}

}
}
