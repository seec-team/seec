//===- ProcessTimeControl.cpp ---------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "OpenTrace.hpp"
#include "ProcessTimeControl.hpp"

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_ostream.h"

IMPLEMENT_CLASS(ProcessTimeEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_PROCESS_TIME_CHANGED, ProcessTimeEvent);
wxDEFINE_EVENT(SEEC_EV_PROCESS_TIME_VIEWED, ProcessTimeEvent);

IMPLEMENT_DYNAMIC_CLASS(ProcessTimeControl, wxPanel);

enum ControlIDs {
  ProcessTimeControl_Reset = wxID_HIGHEST,
  ProcessTimeControl_SlideProcessTime
};

BEGIN_EVENT_TABLE(ProcessTimeControl, wxPanel)
  EVT_COMMAND_SCROLL_CHANGED(ProcessTimeControl_SlideProcessTime,
                             ProcessTimeControl::OnSlideProcessTimeChanged)
END_EVENT_TABLE()

bool ProcessTimeControl::Create(wxWindow *Parent, wxWindowID ID) {
  if (!wxPanel::Create(Parent, ID))
    return false;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Create a slider to control the current process time.
  SlideProcessTime = new wxSlider(this,
                                  ProcessTimeControl_SlideProcessTime,
                                  0, // Value
                                  0, // MinValue
                                  0, // MaxValue
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  wxSL_HORIZONTAL | wxSL_LABELS);

  auto Caption = seec::getwxStringExOrDie(TextTable, "ScrollProcessTime_Title");
  SlideProcessTime->SetLabel(Caption);
  SlideProcessTime->Enable(false); // Disable the slider.

  // Make the slider grow to fill this panel.
  auto TopSizer = new wxGridSizer(1, 1, wxSize(0,0));
  TopSizer->Add(SlideProcessTime, wxSizerFlags().Expand());
  SetSizerAndFit(TopSizer);

  return true;
}

void ProcessTimeControl::setTrace(OpenTrace &TraceData) {
  Trace = &TraceData;
  SlideProcessTime->SetValue(0);
  SlideProcessTime->SetRange(0, Trace->getProcessTrace().getFinalProcessTime());
  SlideProcessTime->Enable(true); // Enable the slider.
}

void ProcessTimeControl::clearTrace() {
  Trace = nullptr;
  SlideProcessTime->SetValue(0);
  SlideProcessTime->SetRange(0, 0);
  SlideProcessTime->Enable(false); // Disable the slider.
}

void ProcessTimeControl::OnSlideProcessTimeChanged(wxScrollEvent& Event) {
  uint64_t Time = Event.GetPosition();
  ProcessTimeEvent Ev(SEEC_EV_PROCESS_TIME_CHANGED, GetId(), Time);
  Ev.SetEventObject(this);
  ProcessWindowEvent(Ev);
}