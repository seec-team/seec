//===- include/seec/Trace/TraceStreams.hpp -------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACESTREAMS_HPP
#define SEEC_TRACE_TRACESTREAMS_HPP

#include "seec/Trace/TraceFormat.hpp"

#include <cstdio>
#include <map>
#include <mutex>

namespace seec {

namespace trace {


/// \brief Information about a physical stream target (source/sink).
///
class TraceFile {
public:
  /// \brief Default constructor.
  TraceFile() {}
  
  /// \brief Copy constructor.
  TraceFile(TraceFile const &) = default;
  
  /// \brief Move constructor.
  TraceFile(TraceFile &&) = default;
  
  /// \brief Copy assignment.
  TraceFile &operator=(TraceFile const &) = default;
  
  /// \brief Move assignment.
  TraceFile &operator=(TraceFile &&) = default;
};


/// \brief Information about a single I/O stream (i.e. a FILE *).
///
class TraceStream {
  /// The offset of the filename string in the trace's data file.
  offset_uint FilenameOffset;
  
  /// The offset of the mode string in the trace's data file.
  offset_uint ModeOffset;
  
public:
  /// \brief Default constructor.
  ///
  TraceStream(offset_uint const WithFilenameOffset,
              offset_uint const WithModeOffset)
  : FilenameOffset(WithFilenameOffset),
    ModeOffset(WithModeOffset)
  {}
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the offset of the filename string in the trace's data file.
  ///
  offset_uint getFilenameOffset() const { return FilenameOffset; }
  
  /// \brief Get the offset of the mode string in the trace's data file.
  ///
  offset_uint getModeOffset() const { return ModeOffset; }
  
  /// @} (Accessors)
};


/// \brief Store information about I/O streams.
///
class TraceStreams {
  /// Map of all open streams.
  std::map<FILE *, TraceStream> Streams;
  
public:
  /// \brief Default constructor.
  ///
  TraceStreams()
  : Streams()
  {}
  
  /// \brief Notify that a stream has been opened.
  ///
  void streamOpened(FILE *Stream,
                    offset_uint const FilenameOffset,
                    offset_uint const ModeOffset);
  
  /// \brief Notify that a stream will be closed.
  ///
  /// \return true iff this stream exists and can be closed successfully.
  ///
  bool streamWillClose(FILE *Stream) const;
  
  /// \brief Get stream information if it exists, otherwise nullptr.
  ///
  TraceStream const *streamInfo(FILE *Stream) const;
  
  /// \brief Notify that a stream was closed.
  ///
  void streamClosed(FILE *Stream);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTREAMS_HPP
