//===- include/seec/Trace/TraceFormatBasic.hpp ---------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACEFORMATBASIC_HPP
#define SEEC_TRACE_TRACEFORMATBASIC_HPP

#include <cstdint>
#include <limits>

namespace seec {

namespace trace {


/// Type used for offsets into trace files.
typedef uint64_t offset_uint;

/// Value used to represent an invalid or nonexistant offset.
inline offset_uint noOffset() {
  return std::numeric_limits<offset_uint>::max();
}

/// Version of the trace storage format.
constexpr inline uint64_t formatVersion() { return 8; }

/// ThreadID used to indicate that an event location refers to the initial
/// state of the process.
constexpr inline uint32_t initialDataThreadID() { return 0; }

/// ProcessTime used to refer to the initial state of the process.
constexpr inline uint64_t initialDataProcessTime() { return 0; }


/// The large-blocks that make up a trace file.
enum class BlockType : uint8_t {
  Empty = 0,
  ModuleBitcode = 1,
  ProcessTrace = 2,
  ProcessData = 3,
  ThreadEvents = 4,
  SignalInfo = 5
};


/// \brief Holds the thread and offset of an event record.
///
class EventLocation {
  /// The thread that contains the event.
  uint32_t ThreadID;
  
  /// The offset of the event in the thread's event trace.
  offset_uint Offset;
  
public:
  /// Construct a new empty EventLocation.
  EventLocation()
  : ThreadID(0),
    Offset(noOffset())
  {}
  
  /// Construct a new EventLocation.
  EventLocation(uint32_t EventThreadID, offset_uint EventOffset)
  : ThreadID(EventThreadID),
    Offset(EventOffset)
  {}
  
  EventLocation(EventLocation const &) = default;
  
  EventLocation &operator=(EventLocation const &) = default;
  
  /// Get the thread ID of the thread that contains the event.
  uint32_t getThreadID() const { return ThreadID; }
  
  /// Check whether this event location has a legitimate offset.
  bool hasOffset() const { return Offset != noOffset(); }
  
  /// Get the offset of the event in the thread's event trace.
  offset_uint getOffset() const { return Offset; }

  /// \name Comparison
  /// @{

  /// \brief Check if this EventLocation is equal to another EventLocation.
  bool operator==(EventLocation const &RHS) const {
    return ThreadID == RHS.ThreadID && Offset == RHS.Offset;
  }

  /// \brief Check if this EventLocation differs from another EventLocation.
  bool operator!=(EventLocation const &RHS) const {
    return !operator==(RHS);
  }

  /// @} (Comparison)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEFORMATBASIC_HPP
