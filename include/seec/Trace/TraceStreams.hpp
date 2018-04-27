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


/// \brief Information about a single I/O stream (i.e. a FILE *).
///
class TraceStream {
  /// The offset of the filename string in the trace's data file.
  llvm::Optional<off_t> m_FilenameOffset;
  
  /// The offset of the mode string in the trace's data file.
  llvm::Optional<off_t> m_ModeOffset;
  
public:
  /// \brief Default constructor.
  ///
  TraceStream(llvm::Optional<off_t> WithFilenameOffset,
              llvm::Optional<off_t> WithModeOffset)
  : m_FilenameOffset(WithFilenameOffset),
    m_ModeOffset(WithModeOffset)
  {}
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the offset of the filename string in the trace's data file.
  ///
  llvm::Optional<off_t> const &getFilenameOffset() const {
    return m_FilenameOffset;
  }
  
  /// \brief Get the offset of the mode string in the trace's data file.
  ///
  llvm::Optional<off_t> const &getModeOffset() const { return m_ModeOffset; }
  
  /// @} (Accessors)
};


/// \brief Information about a single DIR.
///
class TraceDIR {
  /// The offset of the dirname string in the trace's data file.
  llvm::Optional<off_t> m_DirnameOffset;
  
public:
  /// \brief Default constructor.
  ///
  TraceDIR(llvm::Optional<off_t> WithDirnameOffset)
  : m_DirnameOffset(WithDirnameOffset)
  {}
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the offset of the dirname string in the trace's data file.
  ///
  llvm::Optional<off_t> const &getDirnameOffset() const {
    return m_DirnameOffset;
  }
  
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
  
  
  /// \name FILE streams.
  /// @{
  
  /// \brief Notify that a stream has been opened.
  ///
  void streamOpened(FILE *Stream,
                    llvm::Optional<off_t> FilenameOffset,
                    llvm::Optional<off_t> ModeOffset);
  
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
  
  /// @} (FILE streams)
};


/// \brief Store information about DIR pointers.
///
class TraceDirs {
  /// Map of all open DIRs.
  std::map<uintptr_t, TraceDIR> Dirs;
  
public:
  /// \brief Default constructor.
  ///
  TraceDirs()
  : Dirs()
  {}
  
  
  /// \name DIRs.
  /// @{
  
  /// \brief Notify that a DIR has been opened.
  ///
  void DIROpened(void const *TheDIR,
                 llvm::Optional<off_t> DirnameOffset);
  
  /// \brief Notify that a DIR will be closed.
  ///
  /// \return true iff this DIR exists and can be closed successfully.
  ///
  bool DIRWillClose(void const *TheDIR) const;
  
  /// \brief Get DIR information if it exists, otherwise nullptr.
  ///
  TraceDIR const *DIRInfo(void const *TheDIR) const;
  
  /// \brief Notify that a DIR was closed.
  ///
  void DIRClosed(void const *TheDIR);
  
  /// @} (DIRs)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTREAMS_HPP
