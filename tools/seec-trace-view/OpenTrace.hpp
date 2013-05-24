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
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>

/// \brief Holds information for a currently-open SeeC trace.
///
/// Holds all the information for a currently-open SeeC trace, including the
/// llvm::Module responsible for the trace, and the SeeC-Clang mapping to the
/// original source code.
///
class OpenTrace
{
  /// The SeeC-Clang Mapped process trace.
  std::unique_ptr<seec::cm::ProcessTrace> Trace;

  /// \brief Constructor.
  ///
  OpenTrace(std::unique_ptr<seec::cm::ProcessTrace> WithTrace)
  : Trace(std::move(WithTrace))
  {}

  // Don't allow copying.
  OpenTrace(OpenTrace const &) = delete;
  OpenTrace &operator=(OpenTrace const &) = delete;

public:
  /// \brief Destructor.
  ///
  ~OpenTrace() = default;

  /// \brief Attempt to read a trace at the given FilePath.
  /// \param FilePath the path to the process trace file.
  /// \return a seec::Maybe. If the trace was successfully read, then the
  ///         first element will be active and will contain a std::unique_ptr
  ///         holding an OpenTrace. If an error occurred, then the second
  ///         element will be active and will contain the error.
  static
  seec::Maybe<std::unique_ptr<OpenTrace>, seec::Error>
  FromFilePath(wxString const &FilePath);


  /// \name Accessors
  /// @{

  /// \brief Get the mapped process trace.
  seec::cm::ProcessTrace const &getTrace() const { return *Trace; }

  /// @}
};

#endif // SEEC_TRACE_VIEW_OPENTRACE_HPP
