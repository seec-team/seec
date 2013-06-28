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

class OpenTrace;
class SourceViewerPanel;
class StateViewerPanel;
class ThreadMoveEvent;
class ThreadTimeControl;


/// \brief Controls access to a state.
///
class StateAccessToken
{
  /// Indicates whether or not it is legal to access the state using this token.
  bool Valid;
  
  /// Controls access to the state.
  mutable std::mutex AccessMutex;
  
public:
  /// \brief Create a new, valid, access token.
  ///
  StateAccessToken()
  : Valid{true}
  {}
  
  /// \brief Acquire access to read from the associated state.
  ///
  std::unique_lock<std::mutex> getAccess() const {
    std::unique_lock<std::mutex> Lock {AccessMutex};
    
    if (!Valid)
      Lock.unlock();
    
    return Lock;
  }
  
  /// \brief Invalidate this token.
  ///
  void invalidate() {
    // Ensure that no other threads are accessing when we invalidate.
    std::lock_guard<std::mutex> Lock{AccessMutex};
    Valid = false;
  }
};


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

  /// Shows source code.
  SourceViewerPanel *SourceViewer;

  /// Shows the current state.
  StateViewerPanel *StateViewer;
  
  
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

  /// \brief Handle a request to move a thread.
  ///
  void OnThreadTimeMove(ThreadMoveEvent &Event);
};

#endif // SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
