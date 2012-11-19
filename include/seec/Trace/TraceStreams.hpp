//===- include/seec/Trace/TraceStreams.hpp -------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_TRACESTREAMS_HPP
#define SEEC_TRACE_TRACESTREAMS_HPP

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
public:
  /// \brief Default constructor.
  TraceStream() {}
  
  /// \brief Copy constructor.
  TraceStream(TraceStream const &) = default;
  
  /// \brief Move constructor.
  TraceStream(TraceStream &&) = default;
  
  /// \brief Copy assignment.
  TraceStream &operator=(TraceStream const &) = default;
  
  /// \brief Move assignment.
  TraceStream &operator=(TraceStream &&) = default;
};


/// \brief Store information about I/O streams.
///
class TraceStreams {
  /// Map of all open streams.
  std::map<FILE *, TraceStream> Streams;
  
  /// Control access to Streams.
  std::mutex StreamsMutex;
  
public:
  /// \brief Default constructor.
  TraceStreams()
  : Streams()
  {}
  
  /// \brief Notify that a stream has been opened.
  void streamOpened(FILE *stream);
  
  /// \brief Notify that a stream will be closed.
  ///
  /// \return true iff this stream exists and can be closed successfully.
  bool streamWillClose(FILE *stream);
  
  /// \brief Notify that a stream was closed.
  void streamClosed(FILE *stream);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTREAMS_HPP
