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

#include "llvm/ADT/DenseMap.h"

#include <unicode/resbund.h>

#include <memory>
#include <mutex>


namespace seec {
  namespace cm {
    class ProcessState;
  }

  class Resource;
}

class ActionRecord;
class ActionRecordingControl;
class ActionReplayFrame;
class ContextNotifier;
class ExplanationViewer;
class OpenTrace;
class ProcessMoveEvent;
class SourceViewerPanel;
class StateAccessToken;
class StateEvaluationTreePanel;
class StateGraphViewerPanel;
class StreamStatePanel;
class ThreadMoveEvent;
class ThreadTimeControl;
class wxAuiManager;
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

  /// Manages the layout of the individual panels.
  wxAuiManager *Manager;
  
  /// Shows source code.
  SourceViewerPanel *SourceViewer;
  
  /// Shows an explanation of the active Stmt.
  ExplanationViewer *ExplanationCtrl;
  
  /// Shows a graph of the state.
  StateGraphViewerPanel *GraphViewer;
  
  /// Shows an evaluation tree.
  StateEvaluationTreePanel *EvaluationTree;
  
  /// Shows data written to open FILE streams.
  StreamStatePanel *StreamState;

  /// Allows the user to enable/disable action recording.
  ActionRecordingControl *RecordingControl;
  
  /// Used to record user interactions.
  std::unique_ptr<ActionRecord> Recording;
  
  /// Used to replay user interactions.
  ActionReplayFrame *Replay;

  /// Map from windows to their associated menu item in the view menu.
  llvm::DenseMap<wxWindow const *, wxMenuItem *> ViewMenuLookup;
  
  
  /// \name Multi-threaded traces
  /// @{
  
  /// Controls the process time (in multi-threaded traces).
  
  /// @} (Multi-threaded traces)
  
  
  /// \name Single-threaded traces
  /// @{
  
  /// Controls the thread time (in single-threaded traces).
  ThreadTimeControl *ThreadTime;
  
  /// @} (Single-threaded traces)
  
  
  /// \brief Create a single button in the view control menu.
  ///
  void createViewButton(wxMenu &Menu,
                        wxWindow *Window,
                        seec::Resource const &Table,
                        char const * const Key);

  /// \brief Create a view control menu.
  ///
  std::pair<std::unique_ptr<wxMenu>, wxString> createViewMenu();

  /// \brief Create the tools menu.
  ///
  std::pair<std::unique_ptr<wxMenu>, wxString> createToolsMenu();

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

  /// \brief Request to save the current trace.
  ///
  void OnSaveAs(wxCommandEvent &Event);
  
  /// \brief Handle a request to move the process.
  ///
  void OnProcessMove(ProcessMoveEvent &Event);

  /// \brief Handle a request to move a thread.
  ///
  void OnThreadMove(ThreadMoveEvent &Event);
  
  
  /// \name Accessors.
  /// @{
  
  ActionRecord &getRecording() { return *Recording; }
  
  ActionReplayFrame *getReplay() { return Replay; }

  OpenTrace &getTrace() { return *Trace; }
  
  /// @} (Accessors.)
};

#endif // SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
