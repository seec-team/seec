//===- TraceViewerFrame.hpp -----------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
#define SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP

#include "OpenTrace.hpp"
#include "ProcessTimeControl.hpp"
#include "SourceViewer.hpp"
#include "StateViewer.hpp"

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>

class TraceViewerFrame : public wxFrame
{
  /// Stores information about the currently open trace.
  std::unique_ptr<OpenTrace> Trace;

  /// Stores the current ProcessState.
  std::unique_ptr<seec::trace::ProcessState> State;

  /// Controls the current ProcessTime.
  ProcessTimeControl *ProcessTime;

  /// Shows source code.
  SourceViewerPanel *SourceViewer;

  /// Shows the current state.
  StateViewerPanel *StateViewer;

public:
  TraceViewerFrame()
  : Trace(),
    State(),
    ProcessTime(nullptr),
    SourceViewer(nullptr),
    StateViewer(nullptr)
  {}

  TraceViewerFrame(wxWindow *Parent,
                   wxWindowID ID = wxID_ANY,
                   wxString const &Title = wxString(),
                   wxPoint const &Position = wxDefaultPosition,
                   wxSize const &Size = wxDefaultSize)
  : Trace(),
    State(),
    ProcessTime(nullptr),
    SourceViewer(nullptr),
    StateViewer(nullptr)
  {
    Create(Parent, ID, Title, Position, Size);
  }

  ~TraceViewerFrame() {}

  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxString const &Title = wxString(),
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  void OnQuit(wxCommandEvent &Event);
  void OnOpenTrace(wxCommandEvent &Event);

  void OnProcessTimeChanged(ProcessTimeEvent& Event);

private:
  DECLARE_EVENT_TABLE()

  enum CommandEvent {
    ID_Quit = 1, // On Mac we can't a MenuItem ID of Zero.
    ID_OpenTrace
  };
};

#endif // SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
