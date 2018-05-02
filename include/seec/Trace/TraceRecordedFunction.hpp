//===- include/seec/Trace/TraceRecordedFunction.hpp ----------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACERECORDEDFUNCTION_HPP
#define SEEC_TRACE_TRACERECORDEDFUNCTION_HPP

#include "seec/Trace/TraceEventWriter.hpp"
#include "seec/Trace/TraceFormat.hpp"

namespace seec {

namespace trace {


/// \brief Stores the record information for an executed Function.
///
///
class RecordedFunction {
  /// Allows us to rewrite the FunctionStart event when we finish the fn.
  EventWriter::EventWriteRecord<EventType::FunctionStart> StartEventWrite;
  
  /// Index of the Function in the LLVM Module.
  uint32_t Index;

  /// Offset of the FunctionStart event for this function trace.
  offset_uint EventOffsetStart;

  /// Offset of the FunctionEnd event for this function trace.
  offset_uint EventOffsetEnd;

  /// Thread time at which this function was entered.
  uint64_t ThreadTimeEntered;

  /// Thread time at which this function was exited.
  uint64_t ThreadTimeExited;

public:
  /// \brief Constructor.
  ///
  RecordedFunction(
    uint32_t const WithIndex,
    EventWriter::EventWriteRecord<EventType::FunctionStart> Write,
    uint64_t const WithThreadTimeEntered
  )
  : StartEventWrite(Write),
    Index(WithIndex),
    EventOffsetStart(Write.Offset),
    EventOffsetEnd(0),
    ThreadTimeEntered(WithThreadTimeEntered),
    ThreadTimeExited(0)
  {}

  /// Get the index of the Function in the Module.
  uint32_t getIndex() const { return Index; }

  /// Get the offset of the FunctionStart record in the thread's event trace.
  offset_uint getEventOffsetStart() const { return EventOffsetStart; }

  /// Get the offset of the FunctionEnd record in the thread's event trace.
  offset_uint getEventOffsetEnd() const { return EventOffsetEnd; }

  /// Get the thread time at which this Function started recording.
  uint64_t getThreadTimeEntered() const { return ThreadTimeEntered; }

  /// Get the thread time at which this Function finished recording.
  uint64_t getThreadTimeExited() const { return ThreadTimeExited; }

  void setCompletion(EventWriter &Writer,
                     offset_uint const WithEventOffsetEnd,
                     uint64_t const WithThreadTimeExited);
};


} // namespace trace (in seec)
} // namespace seec

#endif // SEEC_TRACE_TRACERECORDEDFUNCTION_HPP
