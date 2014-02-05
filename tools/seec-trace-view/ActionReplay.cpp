//===- tools/seec-trace-view/ActionReplay.cpp -----------------------------===//
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

#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Parsing.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "ActionReplay.hpp"

#include <wx/gauge.h>

#include <string>


//------------------------------------------------------------------------------
// IEventHandler
//------------------------------------------------------------------------------

IEventHandler::~IEventHandler()
{}

seec::Error IEventHandler::error_attribute(std::string const &Name) const
{
  return seec::Error{
    seec::LazyMessageByRef::create("TraceViewer",
                                   {"ActionRecording", "ErrorAttribute"},
                                   std::make_pair("name", Name.c_str()))
  };
}


//------------------------------------------------------------------------------
// ActionReplayFrame
//------------------------------------------------------------------------------

void ActionReplayFrame::ReplayEvent()
{
  assert(NextEvent);
  
  auto const Handler = NextEvent->GetAttribute("handler").ToStdString();
  auto const HandlerIt = Handlers.find(Handler);
  if (HandlerIt == Handlers.end()) {
    wxLogDebug("Handler \"%s\" not found.", Handler);
    return;
  }
  
  // Dispatch to the handler.
  auto const MaybeError = HandlerIt->second->handle(*NextEvent);
  if (MaybeError.assigned<seec::Error>()) {
    wxLogDebug("Handler failed to replay event.");
    return;
  }
}

void ActionReplayFrame::MoveToNextEvent()
{
  if (!NextEvent)
    return;
  
  NextEvent = NextEvent->GetNext();
  
  GaugeEventProgress->SetValue(GaugeEventProgress->GetValue() + 1);
  
  if (!NextEvent) {
    ButtonPlay->Disable();
    ButtonPause->Disable();
    ButtonStep->Disable();
  }
}

void ActionReplayFrame::OnPlay(wxCommandEvent &)
{
  SetEventTimer();
}

void ActionReplayFrame::OnPause(wxCommandEvent &)
{
  if (EventTimer.IsRunning()) {
    EventTimer.Stop();
    ButtonPlay->Enable();
    ButtonPause->Disable();
  }
}

void ActionReplayFrame::OnStep(wxCommandEvent &)
{
  assert(NextEvent);
  
  ReplayEvent();
  MoveToNextEvent();
}

void ActionReplayFrame::SetEventTimer()
{
  assert(NextEvent && !EventTimer.IsRunning());
  
  ButtonPlay->Disable();
  ButtonPause->Enable();
  
  auto const NextTimeStr = NextEvent->GetAttribute("time").ToStdString();
  uint64_t NextTime = 1;
  if (!seec::parseTo(NextTimeStr, NextTime)) {
    wxLogDebug("Couldn't get time for next event.");
  }
  
  EventTimer.Start(NextTime - LastEventTime, wxTIMER_ONE_SHOT);
}

void ActionReplayFrame::OnEventTimer(wxTimerEvent &)
{
  ReplayEvent();
  MoveToNextEvent();
  
  if (NextEvent)
    SetEventTimer();
}

ActionReplayFrame::ActionReplayFrame()
: ButtonPlay(nullptr),
  ButtonPause(nullptr),
  ButtonStep(nullptr),
  GaugeEventProgress(nullptr),
  Handlers(),
  RecordDocument(seec::makeUnique<wxXmlDocument>()),
  NextEvent(nullptr),
  LastEventTime(0),
  EventTimer()
{}

ActionReplayFrame::ActionReplayFrame(wxWindow *Parent)
: ActionReplayFrame()
{
  Create(Parent);
}

ActionReplayFrame::~ActionReplayFrame()
{}

bool ActionReplayFrame::Create(wxWindow *Parent)
{
  // Get the internationalized resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto ICUTable = seec::getResource("TraceViewer",
                                    Locale::getDefault(),
                                    Status,
                                    "ActionRecording");
  if (U_FAILURE(Status))
    return false;
  
  // Create the underlying wxFrame.
  auto const Title = seec::getwxStringExOrEmpty(ICUTable, "ReplayFrameTitle");
  if (!wxFrame::Create(Parent, wxID_ANY, Title))
    return false;
  
  auto const SizerTopLevel = new wxBoxSizer(wxVERTICAL);
  
  // Add buttons for play, pause, step.
  auto const SizerForButtons = new wxBoxSizer(wxHORIZONTAL);
  
  auto const PlayText  = seec::getwxStringExOrEmpty(ICUTable, "ButtonPlay");
  auto const PauseText = seec::getwxStringExOrEmpty(ICUTable, "ButtonPause");
  auto const StepText  = seec::getwxStringExOrEmpty(ICUTable, "ButtonStep");
  
  ButtonPlay  = new wxButton(this, wxID_ANY, PlayText);
  ButtonPause = new wxButton(this, wxID_ANY, PauseText);
  ButtonStep  = new wxButton(this, wxID_ANY, StepText);
  
  ButtonPlay->Bind(wxEVT_BUTTON, &ActionReplayFrame::OnPlay, this);
  ButtonPause->Bind(wxEVT_BUTTON, &ActionReplayFrame::OnPause, this);
  ButtonStep->Bind(wxEVT_BUTTON, &ActionReplayFrame::OnStep, this);
  
  SizerForButtons->Add(ButtonPlay,  wxSizerFlags());
  SizerForButtons->Add(ButtonPause, wxSizerFlags());
  SizerForButtons->Add(ButtonStep,  wxSizerFlags());
  
  SizerTopLevel->Add(SizerForButtons, wxSizerFlags());
  
  // Add the progress gauge.
  GaugeEventProgress = new wxGauge(this, wxID_ANY, 1);
  SizerTopLevel->Add(GaugeEventProgress, wxSizerFlags().Expand());
  
  SetSizerAndFit(SizerTopLevel);
  
  // Bind the close event to hide the frame (only destroy it if the parent is
  // being closed).
  Bind(wxEVT_CLOSE_WINDOW,
      std::function<void (wxCloseEvent &)>{
        [this] (wxCloseEvent &Event) {
          if (Event.CanVeto()) {
            Event.Veto();
            Hide();
          }
          else {
            Event.Skip();
          }
        }});
  
  EventTimer.Bind(wxEVT_TIMER, &ActionReplayFrame::OnEventTimer, this);
  
  return true;
}

std::size_t countChildren(wxXmlNode *Node)
{
  if (!Node)
    return 0;
  
  std::size_t Count = 0;
  for (auto Child = Node->GetChildren(); Child; Child = Child->GetNext())
    ++Count;
  
  return Count;
}

bool ActionReplayFrame::LoadRecording(wxXmlDocument const &Recording)
{
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "ActionRecording");
  assert(U_SUCCESS(Status));
  
  // Copy the recording.
  RecordDocument = seec::makeUnique<wxXmlDocument>(Recording);
  
  auto const Root = RecordDocument->GetRoot();
  if (!Root || Root->GetName() != "recording") {
    auto const ErrorMessage =
      seec::getwxStringExOrEmpty(TextTable, "ReplayFileInvalid");
    wxMessageDialog ErrorDialog{nullptr, ErrorMessage};
    ErrorDialog.ShowModal();
    return false;
  }
  
  GaugeEventProgress->SetRange(countChildren(Root));
  GaugeEventProgress->SetValue(0);
  NextEvent = Root->GetChildren();
  
  Show();
  
  return true;
}
