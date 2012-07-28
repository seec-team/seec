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
#include "SourceViewer.hpp"
#include "StateViewer.hpp"

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Range.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/Path.h"

#include <unicode/resbund.h>

#include <wx/wx.h>
#include <wx/slider.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>

#include <memory>

class TraceViewerFrame : public wxFrame
{
  /// Managers AUI behaviour.
  wxAuiManager Manager;
  
  /// Stores information about the currently open trace.
  std::unique_ptr<OpenTrace> Trace;
  
  /// Stores the current ProcessState.
  std::unique_ptr<seec::trace::ProcessState> State;
  
  /// Slider used to control the current ProcessTime.
  wxSlider *SlideProcessTime;
  
  /// Shows source code.
  SourceViewerPanel *SourceViewer;

  /// Shows the current state.
  StateViewerPanel *StateViewer;

public:
  TraceViewerFrame(wxString const &Title,
                   wxPoint const &Position,
                   wxSize const &Size);
  
  ~TraceViewerFrame() {
    Manager.UnInit();
  }

#define SEEC_COMMAND_EVENT(EVENT) \
  void On##EVENT(wxCommandEvent &Event);
#include "TraceViewerFrameEvents.def"

  void OnSlideProcessTimeChanged(wxScrollEvent& event);

private:
  DECLARE_EVENT_TABLE()

  enum CommandEvent {
#define SEEC_COMMAND_EVENT(EVENT) \
    ID_##EVENT,
#include "TraceViewerFrameEvents.def"
  };
};

#endif // SEEC_TRACE_VIEW_TRACEVIEWERFRAME_HPP
