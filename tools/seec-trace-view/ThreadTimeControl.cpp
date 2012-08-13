//===- ThreadTimeControl.cpp ----------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_ostream.h"

#include "OpenTrace.hpp"
#include "ThreadTimeControl.hpp"


IMPLEMENT_CLASS(ThreadTimeEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_THREAD_TIME_CHANGED, ThreadTimeEvent);
wxDEFINE_EVENT(SEEC_EV_THREAD_TIME_VIEWED,  ThreadTimeEvent);

IMPLEMENT_DYNAMIC_CLASS(ThreadTimeControl, wxPanel);

enum ControlIDs {
  ThreadTimeControl_Reset = wxID_HIGHEST,
  ThreadTimeControl_SlideThreadTime
};


//------------------------------------------------------------------------------
// event table
//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(ThreadTimeControl, wxPanel)
  EVT_COMMAND_SCROLL(ThreadTimeControl_SlideThreadTime,
                     ThreadTimeControl::OnSlide)
END_EVENT_TABLE()


//------------------------------------------------------------------------------
// method implementations
//------------------------------------------------------------------------------

bool ThreadTimeControl::Create(wxWindow *Parent,
                               OpenTrace &TheTrace,
                               seec::trace::ThreadTrace const &TheThreadTrace,
                               wxWindowID ID) {
  if (!wxPanel::Create(Parent, ID))
    return false;

  Trace = &TheTrace;
  ThreadTrace = &TheThreadTrace;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Find the maximum ThreadTime for this thread.
  auto LastEventTime = seec::trace::lastSuccessfulApply(
                                    ThreadTrace->events(),
                                    [](seec::trace::EventRecordBase const &Ev){
                                      return Ev.getThreadTime();
                                    });
  uint64_t LastTime = LastEventTime.assigned() ? LastEventTime.get<0>() : 0;

  // Create a slider to control the current process time.
  SlideThreadTime = new wxSlider(this,
                                 ThreadTimeControl_SlideThreadTime,
                                 0, // Value
                                 0, // MinValue
                                 LastTime, // MaxValue
                                 wxDefaultPosition,
                                 wxDefaultSize,
                                 wxSL_HORIZONTAL
                                 | wxSL_LABELS // Show labels for value.
                                 | wxSL_AUTOTICKS // Show ticks.
                                 | wxSL_BOTTOM // Show ticks below slider.
                                 );

  // TODO: Format Caption for Thread #?
  auto Caption = seec::getwxStringExOrDie(TextTable, "ScrollThreadTime_Title");
  SlideThreadTime->SetLabel(Caption);
  SlideThreadTime->SetTickFreq(1);
  SlideThreadTime->Enable(true); // Enable the slider.

  // Make the slider grow to fill this panel.
  auto TopSizer = new wxGridSizer(1, 1, wxSize(0,0));
  TopSizer->Add(SlideThreadTime, wxSizerFlags().Expand());
  SetSizerAndFit(TopSizer);

  return true;
}

void ThreadTimeControl::OnSlide(wxScrollEvent &Event) {
  auto Type = Event.GetEventType();
  uint64_t Time = Event.GetPosition();
  auto ThreadID = ThreadTrace->getThreadID();

  if (Type == wxEVT_SCROLL_CHANGED) {
    ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, Time);
    Ev.SetEventObject(this);
    ProcessWindowEvent(Ev);
  }
#if __WXMAC__
  // wxEVT_SCROLL_CHANGED isn't raised by the slider in wxCocoa.
  else if (Type == wxEVT_SCROLL_THUMBRELEASE) {
    ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, Time);
    Ev.SetEventObject(this);
    ProcessWindowEvent(Ev);
  }
#endif
}
