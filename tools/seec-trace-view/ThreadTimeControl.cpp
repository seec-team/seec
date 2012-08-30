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


namespace seec {
  namespace trace {
    class ProcessState;
    class ThreadState;
  } // namespace trace (in seec)
} // namespace seec


IMPLEMENT_CLASS(ThreadTimeEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_THREAD_TIME_CHANGED, ThreadTimeEvent);
wxDEFINE_EVENT(SEEC_EV_THREAD_TIME_VIEWED,  ThreadTimeEvent);

IMPLEMENT_DYNAMIC_CLASS(ThreadTimeControl, wxPanel);

enum ControlIDs {
  ThreadTimeControl_Reset = wxID_HIGHEST,
  ThreadTimeControl_SlideThreadTime,
  ThreadTimeControl_ButtonGoToStart,
  ThreadTimeControl_ButtonStepBack,
  ThreadTimeControl_ButtonStepForward,
  ThreadTimeControl_ButtonGoToNextError,
  ThreadTimeControl_ButtonGoToEnd
};


//------------------------------------------------------------------------------
// event table
//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(ThreadTimeControl, wxPanel)
  EVT_COMMAND_SCROLL(ThreadTimeControl_SlideThreadTime,
                     ThreadTimeControl::OnSlide)
  
  EVT_BUTTON(ThreadTimeControl_ButtonGoToStart,
             ThreadTimeControl::OnGoToStart)
  
  EVT_BUTTON(ThreadTimeControl_ButtonStepBack, ThreadTimeControl::OnStepBack)
  
  EVT_BUTTON(ThreadTimeControl_ButtonStepForward,
             ThreadTimeControl::OnStepForward)
  
  EVT_BUTTON(ThreadTimeControl_ButtonGoToNextError,
             ThreadTimeControl::OnGoToNextError)
  
  EVT_BUTTON(ThreadTimeControl_ButtonGoToEnd, ThreadTimeControl::OnGoToEnd)
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
                                     "GUIText",
                                     "ScrollThreadTime");
  assert(U_SUCCESS(Status));

  // Find the maximum ThreadTime for this thread.
  auto LastTime = ThreadTrace->getFinalThreadTime();
  
  // Create a slider to control the current thread time.
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
  auto Caption = seec::getwxStringExOrDie(TextTable, "Title");
  SlideThreadTime->SetLabel(Caption);
  SlideThreadTime->SetTickFreq(1);
  SlideThreadTime->Enable(true); // Enable the slider.
  
  // Create stepping buttons to control the thread time.
  auto ButtonGoToStart = new wxButton(this,
                                      ThreadTimeControl_ButtonGoToStart,
                                      seec::getwxStringExOrDie(TextTable,
                                                               "GoToStart"));
  
  auto ButtonStepBack = new wxButton(this,
                                     ThreadTimeControl_ButtonStepBack,
                                     seec::getwxStringExOrDie(TextTable,
                                                              "StepBack"));

  auto ButtonStepForward = new wxButton(this,
                                        ThreadTimeControl_ButtonStepForward,
                                        seec::getwxStringExOrDie(
                                          TextTable, "StepForward"));

  auto ButtonGoToNextError = new wxButton(this,
                                          ThreadTimeControl_ButtonGoToNextError,
                                          seec::getwxStringExOrDie(
                                            TextTable, "GoToNextError"));

  auto ButtonGoToEnd = new wxButton(this,
                                    ThreadTimeControl_ButtonGoToEnd,
                                    seec::getwxStringExOrDie(TextTable,
                                                             "GoToEnd"));

  // Position all of our controls.
  auto TopSizer = new wxBoxSizer(wxHORIZONTAL);
  TopSizer->Add(ButtonGoToStart, wxSizerFlags().Centre());
  TopSizer->Add(ButtonStepBack, wxSizerFlags().Centre());
  TopSizer->Add(SlideThreadTime, wxSizerFlags().Proportion(1).Expand());
  TopSizer->Add(ButtonStepForward, wxSizerFlags().Centre());
  TopSizer->Add(ButtonGoToNextError, wxSizerFlags().Centre());
  TopSizer->Add(ButtonGoToEnd, wxSizerFlags().Centre());
  SetSizerAndFit(TopSizer);

  return true;
}

void ThreadTimeControl::show(seec::trace::ProcessState &ProcessState,
                             seec::trace::ThreadState &ThreadState) {
  this->ThreadState = &ThreadState;
  
  // If the state's thread time doesn't match our thread time, we must update
  // ourself.
  
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

void ThreadTimeControl::OnGoToStart(wxCommandEvent &WXUNUSED(Event)) {
  if (SlideThreadTime->GetValue() == 0)
    return;
  
  SlideThreadTime->SetValue(0);
  
  auto const ThreadID = ThreadTrace->getThreadID();
  ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, 0);
  Ev.SetEventObject(this);
  ProcessWindowEvent(Ev);
}

void ThreadTimeControl::OnStepBack(wxCommandEvent &WXUNUSED(Event)) {
  if (SlideThreadTime->GetValue() == 0)
    return;
  
  auto const Time = SlideThreadTime->GetValue() - 1;
  SlideThreadTime->SetValue(Time);
  
  auto const ThreadID = ThreadTrace->getThreadID();
  ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, Time);
  Ev.SetEventObject(this);
  ProcessWindowEvent(Ev);
}

void ThreadTimeControl::OnStepForward(wxCommandEvent &WXUNUSED(Event)) {
  auto const MaxValue = SlideThreadTime->GetMax();
  
  if (SlideThreadTime->GetValue() == MaxValue)
    return;
  
  auto const Time = SlideThreadTime->GetValue() + 1;
  SlideThreadTime->SetValue(Time);
  
  auto const ThreadID = ThreadTrace->getThreadID();
  ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, Time);
  Ev.SetEventObject(this);
  ProcessWindowEvent(Ev);
}

void ThreadTimeControl::OnGoToNextError(wxCommandEvent &WXUNUSED(Event)) {
  if (SlideThreadTime->GetValue() == SlideThreadTime->GetMax())
    return;
  
  assert(ThreadState);
  
  auto NextEv = ThreadState->getNextEvent();
  auto SearchRange = seec::trace::rangeAfterIncluding(ThreadTrace->events(),
                                                      NextEv);
  
  // Find the first RuntimeError in SearchRange.
  auto MaybeEvRef = seec::trace::find<seec::trace::EventType::RuntimeError>
                                     (SearchRange);
  if (!MaybeEvRef.assigned()) {
    // There are no more errors.
    return;
  }
  
  // Find the thread time at this position.
  auto TimeSearchRange = seec::trace::rangeBefore(ThreadTrace->events(),
                                                  MaybeEvRef.get<0>());
  
  auto LastEventTime = seec::trace::lastSuccessfulApply(
                                    TimeSearchRange,
                                    [](seec::trace::EventRecordBase const &Ev){
                                      return Ev.getThreadTime();
                                    });
  
  uint64_t LastTime = LastEventTime.assigned() ? LastEventTime.get<0>() : 0;
  
  SlideThreadTime->SetValue(LastTime);
  
  auto const ThreadID = ThreadTrace->getThreadID();
  ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, LastTime);
  Ev.SetEventObject(this);
  ProcessWindowEvent(Ev);
}

void ThreadTimeControl::OnGoToEnd(wxCommandEvent &WXUNUSED(Event)) {
  auto const MaxValue = SlideThreadTime->GetMax();
  
  if (SlideThreadTime->GetValue() == MaxValue)
    return;
  
  SlideThreadTime->SetValue(MaxValue);
  
  auto const ThreadID = ThreadTrace->getThreadID();
  ThreadTimeEvent Ev(SEEC_EV_THREAD_TIME_CHANGED, GetId(), ThreadID, MaxValue);
  Ev.SetEventObject(this);
  ProcessWindowEvent(Ev);
}
