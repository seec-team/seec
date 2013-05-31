//===- tools/seec-trace-view/ThreadTimeControl.cpp ------------------------===//
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

#include "seec/Clang/MappedThreadState.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/ImageResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_ostream.h"

#include <wx/bmpbuttn.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "OpenTrace.hpp"
#include "ThreadTimeControl.hpp"
#include "TraceViewerFrame.hpp"


IMPLEMENT_CLASS(ThreadMoveEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_THREAD_MOVE, ThreadMoveEvent);

IMPLEMENT_DYNAMIC_CLASS(ThreadTimeControl, wxPanel);

enum ControlIDs {
  ControlID_Reset = wxID_HIGHEST,
  ControlID_ButtonGoToStart,
  ControlID_ButtonStepBack,
  ControlID_ButtonStepForward,
  ControlID_ButtonGoToNextError,
  ControlID_ButtonGoToEnd
};


//------------------------------------------------------------------------------
// event table
//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(ThreadTimeControl, wxPanel)
  EVT_BUTTON(ControlID_ButtonGoToStart,     ThreadTimeControl::OnGoToStart)
  EVT_BUTTON(ControlID_ButtonStepBack,      ThreadTimeControl::OnStepBack)
  EVT_BUTTON(ControlID_ButtonStepForward,   ThreadTimeControl::OnStepForward)
  EVT_BUTTON(ControlID_ButtonGoToNextError, ThreadTimeControl::OnGoToNextError)
  EVT_BUTTON(ControlID_ButtonGoToEnd,       ThreadTimeControl::OnGoToEnd)
END_EVENT_TABLE()


//------------------------------------------------------------------------------
// method implementations
//------------------------------------------------------------------------------

bool ThreadTimeControl::Create(wxWindow *Parent, wxWindowID ID)
{
  if (!wxPanel::Create(Parent, ID))
    return false;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText",
                                     "ScrollThreadTime");
  assert(U_SUCCESS(Status));

  // Get the GUI images from the TraceViewer ICU resources.
  auto ImageTable = seec::getResource("TraceViewer",
                                      Locale::getDefault(),
                                      Status,
                                      "GUIImages",
                                      "Movement");
  assert(U_SUCCESS(Status));

  // Create stepping buttons to control the thread time.
#define SEEC_BUTTON(NAME, TEXT_KEY, IMAGE_KEY)                                 \
  auto Text##NAME = seec::getwxStringExOrEmpty(TextTable, TEXT_KEY);           \
  auto Img##NAME = seec::getwxImageEx(ImageTable, IMAGE_KEY, Status);          \
  if (Img##NAME.IsOk()) {                                                      \
    Img##NAME.Rescale(100, 50, wxIMAGE_QUALITY_HIGH);                          \
    Button##NAME = new wxBitmapButton(this, ControlID_Button##NAME, Img##NAME);\
  }                                                                            \
  else {                                                                       \
    Button##NAME = new wxButton(this, ControlID_Button##NAME, Text##NAME);     \
  }                                                                            \
  Button##NAME->Disable();
  
  SEEC_BUTTON(GoToStart,     "GoToStart",     "BackwardArrowToBlock")
  SEEC_BUTTON(StepBack,      "StepBack",      "BackwardArrow")
  SEEC_BUTTON(StepForward,   "StepForward",   "ForwardArrow")
  SEEC_BUTTON(GoToNextError, "GoToNextError", "ForwardArrowToError")
  SEEC_BUTTON(GoToEnd,       "GoToEnd",       "ForwardArrowToBlock")
  
#undef SEEC_BUTTON

  // Position all of our controls.
  auto TopSizer = new wxBoxSizer(wxHORIZONTAL);
  TopSizer->AddStretchSpacer(1);
  
  wxSizerFlags ButtonSizer;
  
  TopSizer->Add(ButtonGoToStart,     ButtonSizer);
  TopSizer->Add(ButtonStepBack,      ButtonSizer);
  TopSizer->Add(ButtonStepForward,   ButtonSizer);
  TopSizer->Add(ButtonGoToNextError, ButtonSizer);
  TopSizer->Add(ButtonGoToEnd,       ButtonSizer);
  
  TopSizer->AddStretchSpacer(1);
  SetSizerAndFit(TopSizer);

  return true;
}

ThreadTimeControl::~ThreadTimeControl() = default;

void ThreadTimeControl::show(std::shared_ptr<StateAccessToken> Access,
                             seec::cm::ProcessState const &Process,
                             seec::cm::ThreadState const &Thread,
                             size_t ThreadIndex)
{
  CurrentAccess = std::move(Access);
  CurrentThreadIndex = ThreadIndex;
  
  if (Thread.isAtStart()) {
    ButtonGoToStart->Disable();
    ButtonStepBack->Disable();
  }
  else {
    ButtonGoToStart->Enable();
    ButtonStepBack->Enable();
  }
  
  if (Thread.isAtEnd()) {
    ButtonStepForward->Disable();
    ButtonGoToEnd->Disable();
  }
  else {
    ButtonStepForward->Enable();
    ButtonGoToEnd->Enable();
  }
}

void ThreadTimeControl::OnGoToStart(wxCommandEvent &WXUNUSED(Event)) {
  // TODO.
}

void ThreadTimeControl::OnStepBack(wxCommandEvent &WXUNUSED(Event)) {
  if (!CurrentAccess)
    return;
  
  auto Lock = CurrentAccess->getAccess();
  if (!Lock) // Our token is out of date.
    return;
  
  ThreadMoveEvent Ev {
    SEEC_EV_THREAD_MOVE,
    GetId(),
    CurrentThreadIndex,
    ThreadMoveEvent::DirectionTy::Backward
  };
  
  Ev.SetEventObject(this);
  
  Lock.unlock();
  
  ProcessWindowEvent(Ev);
}

void ThreadTimeControl::OnStepForward(wxCommandEvent &WXUNUSED(Event)) {
  if (!CurrentAccess)
    return;
  
  auto Lock = CurrentAccess->getAccess();
  if (!Lock) // Our token is out of date.
    return;
  
  ThreadMoveEvent Ev {
    SEEC_EV_THREAD_MOVE,
    GetId(),
    CurrentThreadIndex,
    ThreadMoveEvent::DirectionTy::Forward
  };
  
  Ev.SetEventObject(this);
  
  Lock.unlock();
  
  ProcessWindowEvent(Ev);
}

void ThreadTimeControl::OnGoToNextError(wxCommandEvent &WXUNUSED(Event)) {
  // TODO.
}

void ThreadTimeControl::OnGoToEnd(wxCommandEvent &WXUNUSED(Event)) {
  // TODO.
}
