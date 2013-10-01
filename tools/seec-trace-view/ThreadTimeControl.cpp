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
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/ImageResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_ostream.h"

#include <wx/bmpbuttn.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "InternationalizedButton.hpp"
#include "OpenTrace.hpp"
#include "ThreadMoveEvent.hpp"
#include "ThreadTimeControl.hpp"
#include "TraceViewerFrame.hpp"


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

void ThreadTimeControl::disableAll()
{
  if (ButtonGoToStart)
    ButtonGoToStart->Disable();
  
  if (ButtonStepBack)
    ButtonStepBack->Disable();
  
  if (ButtonStepForward)
    ButtonStepForward->Disable();
  
  if (ButtonGoToEnd)
    ButtonGoToEnd->Disable();
}

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
  Button##NAME =                                                               \
    makeInternationalizedButton(this, ControlID_Button##NAME,                  \
                                TextTable, TEXT_KEY,                           \
                                ImageTable, IMAGE_KEY, wxSize(50, 25));        \
  Button##NAME->Disable();
  
  SEEC_BUTTON(GoToStart,     "GoToStart",     "BackwardArrowToBlock")
  SEEC_BUTTON(StepBack,      "StepBack",      "BackwardArrow")
  SEEC_BUTTON(StepForward,   "StepForward",   "ForwardArrow")
  // SEEC_BUTTON(GoToNextError, "GoToNextError", "ForwardArrowToError")
  SEEC_BUTTON(GoToEnd,       "GoToEnd",       "ForwardArrowToBlock")
  
#undef SEEC_BUTTON

  // Position all of our controls.
  auto TopSizer = new wxBoxSizer(wxHORIZONTAL);
  TopSizer->AddStretchSpacer(1);
  
  wxSizerFlags ButtonSizer;
  
  TopSizer->Add(ButtonGoToStart,     ButtonSizer);
  TopSizer->Add(ButtonStepBack,      ButtonSizer);
  TopSizer->Add(ButtonStepForward,   ButtonSizer);
  // TopSizer->Add(ButtonGoToNextError, ButtonSizer);
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
  
  // Setup the backwards movement buttons.
  if (Thread.isAtStart()) {
    ButtonGoToStart->Disable();
    ButtonStepBack->Disable();
  }
  else {
    ButtonGoToStart->Enable();
    ButtonStepBack->Enable();
  }
  
  // Setup the forwards movement buttons.
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
  disableAll();
  
  raiseMovementEvent(*this,
                     CurrentAccess,
                     CurrentThreadIndex,
                     [] (seec::cm::ThreadState &Thread) -> bool {
                        return seec::cm::moveBackwardToEnd(Thread);
                     });
}

void ThreadTimeControl::OnStepBack(wxCommandEvent &WXUNUSED(Event)) {
  disableAll();
  
  raiseMovementEvent(*this,
                     CurrentAccess,
                     CurrentThreadIndex,
                     [] (seec::cm::ThreadState &Thread) -> bool {
                        return seec::cm::moveBackward(Thread);
                     });
}

void ThreadTimeControl::OnStepForward(wxCommandEvent &WXUNUSED(Event)) {
  disableAll();
  
  raiseMovementEvent(*this,
                     CurrentAccess,
                     CurrentThreadIndex,
                     [] (seec::cm::ThreadState &Thread) -> bool {
                        return seec::cm::moveForward(Thread);
                     });
}

void ThreadTimeControl::OnGoToNextError(wxCommandEvent &WXUNUSED(Event)) {
  // TODO.
}

void ThreadTimeControl::OnGoToEnd(wxCommandEvent &WXUNUSED(Event)) {
  disableAll();
  
  raiseMovementEvent(*this,
                     CurrentAccess,
                     CurrentThreadIndex,
                     [] (seec::cm::ThreadState &Thread) -> bool {
                        return seec::cm::moveForwardToEnd(Thread);
                     });
}
