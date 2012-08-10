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

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>

#include "OpenTrace.hpp"
#include "ProcessTimeControl.hpp"
#include "SourceViewer.hpp"
#include "StateViewer.hpp"

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
                   std::unique_ptr<OpenTrace> &&TracePtr,
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
    Create(Parent, std::move(TracePtr), ID, Title, Position, Size);
  }

  /// \brief Destructor.
  ~TraceViewerFrame();

  bool Create(wxWindow *Parent,
              std::unique_ptr<OpenTrace> &&TracePtr,
              wxWindowID ID = wxID_ANY,
              wxString const &Title = wxString(),
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Close the current file.
  void OnClose(wxCommandEvent &Event);

  void OnProcessTimeChanged(ProcessTimeEvent& Event);

private:
  DECLARE_EVENT_TABLE()
};

#endif // SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
