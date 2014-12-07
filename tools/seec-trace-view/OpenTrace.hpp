//===- tools/seec-trace-view/OpenTrace.hpp --------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_OPENTRACE_HPP
#define SEEC_TRACE_VIEW_OPENTRACE_HPP

#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include <wx/wx.h>

#include <memory>
#include <string>


class wxXmlDocument;


/// \brief Holds information for a currently-open SeeC trace.
///
/// Holds all the information for a currently-open SeeC trace, including the
/// llvm::Module responsible for the trace, and the SeeC-Clang mapping to the
/// original source code.
///
class OpenTrace
{
  /// Path to the temporary directory containing trace files, if used.
  std::string TempDir;

  /// Paths for the individual trace files.
  std::vector<std::string> TempFiles;

  /// The SeeC-Clang Mapped process trace.
  std::unique_ptr<seec::cm::ProcessTrace> Trace;

  /// The action recording.
  std::unique_ptr<wxXmlDocument> Recording;

  /// \brief Constructor.
  ///
  OpenTrace(std::string WithTempDir,
            std::vector<std::string> WithTempFiles,
            std::unique_ptr<seec::cm::ProcessTrace> WithTrace,
            std::unique_ptr<wxXmlDocument> WithRecording);
  
  /// \brief Constructor.
  ///
  OpenTrace(std::unique_ptr<seec::cm::ProcessTrace> WithTrace);

  /// \brief Attempt to read a trace from a file or directory.
  ///
  static seec::Maybe<std::unique_ptr<seec::cm::ProcessTrace>, seec::Error>
  ReadTraceFromFilePath(wxString const &FilePath);

  /// \brief Attempt to read a trace and record from a seecrecording archive.
  ///
  static seec::Maybe<std::unique_ptr<OpenTrace>, seec::Error>
  FromRecordingArchive(wxString const &FilePath);

  // Don't allow copying.
  OpenTrace(OpenTrace const &) = delete;
  OpenTrace &operator=(OpenTrace const &) = delete;

public:
  /// \brief Destructor.
  ///
  ~OpenTrace();


  /// \brief Attempt to read a trace at the given FilePath.
  /// \param FilePath the path to the process trace file.
  /// \return a seec::Maybe. If the trace was successfully read, then the
  ///         first element will be active and will contain a std::unique_ptr
  ///         holding an OpenTrace. If an error occurred, then the second
  ///         element will be active and will contain the error.
  ///
  static seec::Maybe<std::unique_ptr<OpenTrace>, seec::Error>
  FromFilePath(wxString const &FilePath);


  /// \name Accessors
  /// @{

  /// \brief Get the mapped process trace.
  ///
  seec::cm::ProcessTrace const &getTrace() const { return *Trace; }
  
  /// \brief Get the action recording associated with this trace, if any.
  ///
  decltype(Recording) const &getRecording() const { return Recording; }

  /// @}
};

#endif // SEEC_TRACE_VIEW_OPENTRACE_HPP
