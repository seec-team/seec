//===- tools/seec-trace-view/TraceViewerFrame.hpp -------------------------===//
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

#ifndef SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
#define SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>
#include <mutex>


namespace seec {
  namespace cm {
    class ProcessState;
  }
}

class ContextNotifier;
class OpenTrace;
class ProcessMoveEvent;
class SourceViewerPanel;
class StateAccessToken;
class StateViewerPanel;
class ThreadMoveEvent;
class ThreadTimeControl;
class wxXmlDocument;


/// \brief Displays a SeeC-Clang Mapped process trace.
///
class TraceViewerFrame : public wxFrame
{
  /// Stores information about the currently open trace.
  std::unique_ptr<OpenTrace> Trace;
  
  /// Stores the process state.
  std::unique_ptr<seec::cm::ProcessState> State;
  
  /// Controls access to the current process state.
  std::shared_ptr<StateAccessToken> StateAccess;
  
  /// Central handler for context notifications.
  std::unique_ptr<ContextNotifier> Notifier;

  /// Shows source code.
  SourceViewerPanel *SourceViewer;

  /// Shows the current state.
  StateViewerPanel *StateViewer;
  
  /// Used to record user interactions.
  std::unique_ptr<wxXmlDocument> ActionRecord;
  
  
  /// \name Multi-threaded traces
  /// @{
  
  /// Controls the process time (in multi-threaded traces).
  
  /// @} (Multi-threaded traces)
  
  
  /// \name Single-threaded traces
  /// @{
  
  /// Controls the thread time (in single-threaded traces).
  ThreadTimeControl *ThreadTime;
  
  /// @} (Single-threaded traces)

public:
  /// \brief Constructor (without creation).
  ///
  TraceViewerFrame();

  /// \brief Constructor (with creation).
  ///
  TraceViewerFrame(wxWindow *Parent,
                   std::unique_ptr<OpenTrace> TracePtr,
                   wxWindowID ID = wxID_ANY,
                   wxString const &Title = wxString(),
                   wxPoint const &Position = wxDefaultPosition,
                   wxSize const &Size = wxDefaultSize);

  /// \brief Destructor.
  ///
  ~TraceViewerFrame();

  /// \brief Create the frame (if it was default-constructed).
  ///
  bool Create(wxWindow *Parent,
              std::unique_ptr<OpenTrace> TracePtr,
              wxWindowID ID = wxID_ANY,
              wxString const &Title = wxString(),
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Close the current file.
  ///
  void OnClose(wxCommandEvent &Event);
  
  /// \brief Handle a request to move the process.
  ///
  void OnProcessMove(ProcessMoveEvent &Event);

  /// \brief Handle a request to move a thread.
  ///
  void OnThreadMove(ThreadMoveEvent &Event);
};

#endif // SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
