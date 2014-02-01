//===- tools/seec-trace-view/TraceViewerFrame.cpp -------------------------===//
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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Path.h"

#include <chrono>
#include <cinttypes>
#include <iostream>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "CommonMenus.hpp"
#include "NotifyContext.hpp"
#include "OpenTrace.hpp"
#include "ProcessMoveEvent.hpp"
#include "SourceViewer.hpp"
#include "StateAccessToken.hpp"
#include "StateViewer.hpp"
#include "ThreadMoveEvent.hpp"
#include "ThreadTimeControl.hpp"
#include "TraceViewerApp.hpp"
#include "TraceViewerFrame.hpp"


TraceViewerFrame::TraceViewerFrame()
: Trace(),
  State(),
  StateAccess(),
  Notifier(),
  SourceViewer(nullptr),
  StateViewer(nullptr),
  Recording(nullptr),
  Replay(nullptr),
  ThreadTime(nullptr)
{}

TraceViewerFrame::TraceViewerFrame(wxWindow *Parent,
                                   std::unique_ptr<OpenTrace> TracePtr,
                                   wxWindowID ID,
                                   wxString const &Title,
                                   wxPoint const &Position,
                                   wxSize const &Size)
: Trace(),
  State(),
  StateAccess(),
  Notifier(),
  SourceViewer(nullptr),
  StateViewer(nullptr),
  Recording(nullptr),
  Replay(nullptr),
  ThreadTime(nullptr)
{
  Create(Parent, std::move(TracePtr), ID, Title, Position, Size);
}

TraceViewerFrame::~TraceViewerFrame() {
  // Notify the TraceViewerApp that we have been destroyed.
  auto &App = wxGetApp();
  App.removeTopLevelWindow(this);
}

bool TraceViewerFrame::Create(wxWindow *Parent,
                              std::unique_ptr<OpenTrace> TracePtr,
                              wxWindowID ID,
                              wxString const &Title,
                              wxPoint const &Position,
                              wxSize const &Size)
{
  if (!wxFrame::Create(Parent, ID, Title, Position, Size))
    return false;

  // Setup the action record.
  Recording = seec::makeUnique<ActionRecord>();
  Recording->enable();
  
  // Setup the action replay frame.
  Replay = new ActionReplayFrame(this);
  
  // Set the trace.
  Trace = std::move(TracePtr);
  
  // Create a new state at the beginning of the trace.
  State = seec::makeUnique<seec::cm::ProcessState>(Trace->getTrace());
  
  // Create a new accessor token for this state.
  StateAccess = std::make_shared<StateAccessToken>();
  
  // Setup the context notifier.
  Notifier = seec::makeUnique<ContextNotifier>();

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Setup the menus.
  auto menuBar = new wxMenuBar();
  append(menuBar, createFileMenu());
  append(menuBar, createRecordingMenu(*this));

  SetMenuBar(menuBar);

  // Setup a status bar.
  CreateStatusBar();

  if (State->getThreadCount() == 1) {
    // Setup the view for a single-threaded trace.

    // Create the thread time movement control.
    ThreadTime = new ThreadTimeControl(this, *Recording, Replay);

    // Create the source code viewer.
    SourceViewer = new SourceViewerPanel(this,
                                         *Trace,
                                         *Notifier);

    // Create a text control to show the current state.
    StateViewer = new StateViewerPanel(this,
                                       *Notifier,
                                       *Recording);

    wxBoxSizer *ParentSizer = new wxBoxSizer(wxVERTICAL);
    ParentSizer->Add(ThreadTime, wxSizerFlags().Expand());

    wxBoxSizer *ViewSizer = new wxBoxSizer(wxHORIZONTAL);
    ViewSizer->Add(SourceViewer, wxSizerFlags().Proportion(1).Expand());
    ViewSizer->Add(StateViewer, wxSizerFlags().Proportion(1).Expand());

    ParentSizer->Add(ViewSizer, wxSizerFlags().Proportion(1).Expand());

    SetSizer(ParentSizer);

    // Display the initial state.
    // StateViewer->show(StateAccess, *State);
    SourceViewer->show(StateAccess, *State, State->getThread(0));
    ThreadTime->show(StateAccess, *State, State->getThread(0), 0);
  }
  else {
    // TODO: Setup the view for a multi-threaded trace.
  }
  
  // Setup the event handling.
  Bind(wxEVT_COMMAND_MENU_SELECTED,
       &TraceViewerFrame::OnClose, this,
       wxID_CLOSE);
  
  Bind(SEEC_EV_PROCESS_MOVE, &TraceViewerFrame::OnProcessMove, this);
  
  Bind(SEEC_EV_THREAD_MOVE, &TraceViewerFrame::OnThreadMove, this);
  
  // Setup action recording.
  Bind(wxEVT_SIZE, std::function<void (wxSizeEvent &)> {
    [this] (wxSizeEvent &Ev) -> void {
      // TODO: Don't record if we're replaying.
      if (Recording) {
        auto const &Size = Ev.GetSize();
        Recording->recordEventL("TraceViewerFrame.Resize",
                                make_attribute("width", Size.GetWidth()),
                                make_attribute("height", Size.GetHeight()));
      }
      
      Ev.Skip();
    }
  });
  
  Replay->RegisterHandler("TraceViewerFrame.Resize",
                          {"width", "height"},
    std::function<void (int, int)>{
      [this] (int width, int height) -> void {
        wxLogDebug("TraceViewerFrame.Resize %d,%d", width, height);
        this->SetSize(width, height);
        this->Layout();
      }});

  return true;
}

void TraceViewerFrame::OnClose(wxCommandEvent &Event) {
  Close(true);
}

void TraceViewerFrame::OnProcessMove(ProcessMoveEvent &Event) {
  // Deny access to the state.
  if (StateAccess)
    StateAccess->invalidate();
  
  // Move the process.
  auto const MoveStart = std::chrono::steady_clock::now();
  
  Event.getMover()(*State);
  
  auto const MoveEnd = std::chrono::steady_clock::now();
  auto const MoveMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (MoveEnd - MoveStart);
  wxLogDebug("Moved process in %" PRIu64 " ms",
             static_cast<uint64_t>(MoveMS.count()));
  
  // Create a new access token for the state.
  StateAccess = std::make_shared<StateAccessToken>();
  
  // Display the new state.
  if (State->getThreadCount() == 1) {
    StateViewer->show(StateAccess, *State, State->getThread(0));
    SourceViewer->show(StateAccess, *State, State->getThread(0));
    ThreadTime->show(StateAccess, *State, State->getThread(0), 0);
  }
  else {
    // TODO: Show the state for a multi-threaded trace.
  }
  
  auto const ShowEnd = std::chrono::steady_clock::now();
  auto const ShowMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (ShowEnd - MoveEnd);
  wxLogDebug("Showed state in %" PRIu64 " ms",
             static_cast<uint64_t>(ShowMS.count()));
}

void TraceViewerFrame::OnThreadMove(ThreadMoveEvent &Event) {
  // Deny access to the state.
  if (StateAccess)
    StateAccess->invalidate();
  
  // Move the thread.
  auto const Index = Event.getThreadIndex();
  auto &Thread = State->getThread(Index);
  
  auto const MoveStart = std::chrono::steady_clock::now();
  
  Event.getMover()(Thread);
  
  auto const MoveEnd = std::chrono::steady_clock::now();
  auto const MoveMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (MoveEnd - MoveStart);
  wxLogDebug("Moved thread in %" PRIu64 " ms",
             static_cast<uint64_t>(MoveMS.count()));
  
  // Create a new access token for the state.
  StateAccess = std::make_shared<StateAccessToken>();
  
  // Display the new state.
  StateViewer->show(StateAccess, *State, State->getThread(Index));
  SourceViewer->show(StateAccess, *State, State->getThread(Index));
  ThreadTime->show(StateAccess, *State, State->getThread(Index), Index);
  
  auto const ShowEnd = std::chrono::steady_clock::now();
  auto const ShowMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (ShowEnd - MoveEnd);
  wxLogDebug("Showed state in %" PRIu64 " ms",
             static_cast<uint64_t>(ShowMS.count()));
}
