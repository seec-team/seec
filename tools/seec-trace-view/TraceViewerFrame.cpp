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
#include "seec/Util/MakeFunction.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Path.h"

#include <wx/config.h>
#include <wx/aui/aui.h>
#include <wx/aui/framemanager.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <chrono>
#include <cinttypes>
#include <iostream>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "CommonMenus.hpp"
#include "ExplanationViewer.hpp"
#include "NotifyContext.hpp"
#include "OpenTrace.hpp"
#include "ProcessMoveEvent.hpp"
#include "SourceViewer.hpp"
#include "StateAccessToken.hpp"
#include "StateEvaluationTree.hpp"
#include "StateGraphViewer.hpp"
#include "StreamStatePanel.hpp"
#include "ThreadMoveEvent.hpp"
#include "ThreadTimeControl.hpp"
#include "TraceViewerApp.hpp"
#include "TraceViewerFrame.hpp"


char const * const cConfigKeyForPerspective = "/TraceViewerFrame/Perspective";
char const * const cConfigKeyForWidth       = "/TraceViewerFrame/Width";
char const * const cConfigKeyForHeight      = "/TraceViewerFrame/Height";


static void createViewButton(wxMenu &Menu,
                             wxAuiManager &Manager,
                             wxWindow *Window,
                             ResourceBundle const &Table,
                             char const * const Key)
{
  // This particular panel was not created for this trace viewer.
  if (!Window)
    return;

  auto &PaneInfo = Manager.GetPane(Window);
  if (!PaneInfo.IsOk())
    return;

  auto const Item = Menu.Append(wxID_ANY, seec::getwxStringExOrKey(Table, Key));
  if (!Item)
    return;

  Menu.Bind(
    wxEVT_MENU,
    std::function<void (wxEvent &)>{
      [Window, &Manager] (wxEvent &) -> void {
        auto &PI = Manager.GetPane(Window);
        if (PI.IsOk()) {
          PI.Show();
          Manager.Update();
        }
      }
    },
    Item->GetId());
}

std::pair<std::unique_ptr<wxMenu>, wxString> TraceViewerFrame::createViewMenu()
{
  UErrorCode Status = U_ZERO_ERROR;
  auto const Text =
    seec::getResource("TraceViewer", Locale::getDefault(), Status,
                      "GUIText", "MenuView");
  if (U_FAILURE(Status))
    return std::make_pair(nullptr, wxEmptyString);
  
  auto Menu = seec::makeUnique<wxMenu>();
  
  createViewButton(*Menu, *Manager, ExplanationCtrl, Text, "Explanation");
  createViewButton(*Menu, *Manager, GraphViewer,     Text, "Graph");
  createViewButton(*Menu, *Manager, EvaluationTree,  Text, "EvaluationTree");
  createViewButton(*Menu, *Manager, StreamState,     Text, "StreamState");
  
  return std::make_pair(std::move(Menu),
                        seec::getwxStringExOrKey(Text, "Title"));
}

TraceViewerFrame::TraceViewerFrame()
: Trace(),
  State(),
  StateAccess(),
  Notifier(),
  SourceViewer(nullptr),
  ExplanationCtrl(nullptr),
  GraphViewer(nullptr),
  EvaluationTree(nullptr),
  StreamState(nullptr),
  RecordingControl(nullptr),
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
: TraceViewerFrame()
{
  Create(Parent, std::move(TracePtr), ID, Title, Position, Size);
}

TraceViewerFrame::~TraceViewerFrame() {
  // Finalize the recording.
  Recording->finalize();

  auto const Config = wxConfig::Get();

  // Save the size of the frame.
  auto const WindowSize = GetSize();
  Config->Write(cConfigKeyForWidth, WindowSize.GetWidth());
  Config->Write(cConfigKeyForHeight, WindowSize.GetHeight());

  // Save the user's perspective.
  auto const Perspective = Manager->SavePerspective();
  Config->Write(cConfigKeyForPerspective, Perspective);

  Config->Flush();

  // Shutdown the AUI manager.
  Manager->UnInit();

  // Notify the TraceViewerApp that we have been destroyed.
  auto &App = wxGetApp();
  App.removeTopLevelWindow(this);
}

bool TraceViewerFrame::Create(wxWindow *Parent,
                              std::unique_ptr<OpenTrace> TracePtr,
                              wxWindowID ID,
                              wxString const &Title,
                              wxPoint const &Position,
                              wxSize const &GivenSize)
{
  // Use the size of the user's last TraceViewerFrame, if available.
  auto const Config = wxConfig::Get();
  auto const Size = wxSize(
    Config->ReadLong(cConfigKeyForWidth,  GivenSize.GetWidth()),
    Config->ReadLong(cConfigKeyForHeight, GivenSize.GetHeight()));

  if (!wxFrame::Create(Parent, ID, Title, Position, Size))
    return false;
  
  // Set the trace.
  Trace = std::move(TracePtr);
  
  // Create a new state at the beginning of the trace.
  State = seec::makeUnique<seec::cm::ProcessState>(Trace->getTrace());
  
  // Create a new accessor token for this state.
  StateAccess = std::make_shared<StateAccessToken>();
  
  // Setup the action record.
  Recording = seec::makeUnique<ActionRecord>(Trace->getTrace());
#ifdef SEEC_USER_ACTION_RECORDING
  Recording->enable();
#endif
  
  // Setup the action replay frame.
  Replay = new ActionReplayFrame(this, Trace->getTrace());
  if (Trace->getRecording()) {
    Replay->LoadRecording(*(Trace->getRecording()));
    Recording->disable();
  }
  
  // Setup the context notifier.
  Notifier = seec::makeUnique<ContextNotifier>();

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));
  
  // Setup the layout manager.
  Manager = new wxAuiManager(this);

  if (State->getThreadCount() == 1) {
    // Setup the view for a single-threaded trace.

#ifdef SEEC_USER_ACTION_RECORDING
    // Create the action recording control.
    RecordingControl = new ActionRecordingControl(this, *Recording);
    auto const RecordingControlTitle =
      seec::getwxStringExOrEmpty("TraceViewer",
                                 (char const *[]){"RecordingToolbar", "Title"});
    Manager->AddPane(RecordingControl,
                     wxAuiPaneInfo().Name("RecordingControl")
                                    .Caption(RecordingControlTitle)
                                    .Top()
                                    .ToolbarPane());
#endif

    // Create the thread time movement control.
    ThreadTime = new ThreadTimeControl(this, *Recording, Replay);
    auto const ThreadTimeTitle =
      seec::getwxStringExOrEmpty(TextTable,
                                 (char const *[]){"ScrollThreadTime", "Title"});
    Manager->AddPane(ThreadTime,
                     wxAuiPaneInfo{}.Name("ThreadTime")
                                    .Caption(ThreadTimeTitle)
                                    .Top()
                                    .ToolbarPane());

    // Create the source code viewer.
    SourceViewer = new SourceViewerPanel(this,
                                         *Trace,
                                         *Notifier,
                                         *Recording,
                                         *Replay);
    auto const SourceViewerTitle =
      seec::getwxStringExOrEmpty(TextTable, "SourceBook_Title");
    Manager->AddPane(SourceViewer,
                     wxAuiPaneInfo{}.Name("SourceViewer")
                                    .Caption(SourceViewerTitle)
                                    .CentrePane());

    // Setup the explanation viewer.
    ExplanationCtrl = new ExplanationViewer(this,
                                            *Notifier,
                                            *Recording,
                                            *Replay);
    auto const ExplanationCtrlTitle =
      seec::getwxStringExOrEmpty(TextTable,
                                 (char const *[]){"Explanation", "Title"});
    Manager->AddPane(ExplanationCtrl,
                     wxAuiPaneInfo{}.Name("ExplanationCtrl")
                                    .Caption(ExplanationCtrlTitle)
                                    .Bottom()
                                    .MinimizeButton(true));

    // Create the evaluation tree.
    EvaluationTree = new StateEvaluationTreePanel(this,
                                                  *Notifier,
                                                  *Recording,
                                                  *Replay);
    auto const EvaluationTreeTitle =
      seec::getwxStringExOrEmpty(TextTable,
                                 (char const *[]){"EvaluationTree", "Title"});
    Manager->AddPane(EvaluationTree,
                     wxAuiPaneInfo{}.Name("EvaluationTree")
                                    .Caption(EvaluationTreeTitle)
                                    .Right()
                                    .MinimizeButton(true)
                                    .MaximizeButton(true));

    // Create the stream viewer.
    StreamState = new StreamStatePanel(this, *Notifier, *Recording, *Replay);
    auto const StreamStateTitle =
      seec::getwxStringExOrEmpty(TextTable,
                                 (char const *[]){"StreamState", "Title"});
    Manager->AddPane(StreamState,
                     wxAuiPaneInfo{}.Name("StreamState")
                                    .Caption(StreamStateTitle)
                                    .Right()
                                    .MinimizeButton(true)
                                    .MaximizeButton(true));

    // Create the graph viewer.
    GraphViewer = new StateGraphViewerPanel(this,
                                            *Notifier,
                                            *Recording,
                                            *Replay);
    auto const GraphViewerTitle =
      seec::getwxStringExOrEmpty(TextTable,
                                 (char const *[]){"Graph", "Title"});
    Manager->AddPane(GraphViewer,
                     wxAuiPaneInfo{}.Name("GraphViewer")
                                    .Caption(GraphViewerTitle)
                                    .Right()
                                    .MinimizeButton(true)
                                    .MaximizeButton(true));

    // Display the initial state. Call SourceViewer's show() last, as it may
    // produce highlight event notifications that the other controls react to.
    auto const &ThreadState = State->getThread(0);
    ThreadTime->show(StateAccess, *State, ThreadState, 0);
    ExplanationCtrl->show(StateAccess, *State, ThreadState);
    EvaluationTree->show(StateAccess, *State, ThreadState);
    // GraphViewer->show(StateAccess, *State, ThreadState);
    SourceViewer->show(StateAccess, *State, ThreadState);
    StreamState->show(StateAccess, *State, ThreadState);
  }
  else {
    // TODO: Setup the view for a multi-threaded trace.
  }

  // Load the user's last-used perspective.
  wxString Perspective;
  if (Config->Read(cConfigKeyForPerspective, &Perspective))
    Manager->LoadPerspective(Perspective, false);

  // Ensure that the unclosable frames are shown.
  if (RecordingControl)
    Manager->GetPane(RecordingControl).Show();
  if (ThreadTime)
    Manager->GetPane(ThreadTime).Show();
  Manager->GetPane(SourceViewer).Show();

  Manager->Update();

  // Setup the menus.
  auto menuBar = new wxMenuBar();
  append(menuBar, createFileMenu());
  append(menuBar, createViewMenu());
  append(menuBar, createRecordingMenu(*this));
  
  // TODO: Setup menu to open/close individual panels.
  
  SetMenuBar(menuBar);
  
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
                          {{"width", "height"}}, seec::make_function(
    [this] (int width, int height) -> void {
      wxLogDebug("TraceViewerFrame.Resize %d,%d", width, height);
      this->SetSize(width, height);
      this->Layout();
    }));

  // Setup replay of contextual navigation.
  registerStmtNavigationReplay(*this, StateAccess, *Replay);

  return true;
}

void TraceViewerFrame::OnClose(wxCommandEvent &Event) {
  Close(true);
}

void TraceViewerFrame::OnProcessMove(ProcessMoveEvent &Event) {
  // Deny future access to the state (this will wait for current readers to
  // complete their work).
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
  
  // Display the new state. Call SourceViewer's show() last, as it may produce
  // highlight event notifications that the other controls react to.
  if (State->getThreadCount() == 1) {
    auto const &ThreadState = State->getThread(0);
    ThreadTime->show(StateAccess, *State, ThreadState, 0);
    ExplanationCtrl->show(StateAccess, *State, ThreadState);
    EvaluationTree->show(StateAccess, *State, ThreadState);
    GraphViewer->show(StateAccess, *State, ThreadState);
    SourceViewer->show(StateAccess, *State, ThreadState);
    StreamState->show(StateAccess, *State, ThreadState);
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
  // Deny future access to the state (this will wait for current readers to
  // complete their work).
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
  
  // Display the new state. Call SourceViewer's show() last, as it may produce
  // highlight event notifications that the other controls react to.
  auto const &ThreadState = State->getThread(Index);
  ThreadTime->show(StateAccess, *State, ThreadState, Index);
  ExplanationCtrl->show(StateAccess, *State, ThreadState);
  EvaluationTree->show(StateAccess, *State, ThreadState);
  GraphViewer->show(StateAccess, *State, ThreadState);
  SourceViewer->show(StateAccess, *State, ThreadState);
  StreamState->show(StateAccess, *State, ThreadState);
  
  auto const ShowEnd = std::chrono::steady_clock::now();
  auto const ShowMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                (ShowEnd - MoveEnd);
  wxLogDebug("Showed state in %" PRIu64 " ms",
             static_cast<uint64_t>(ShowMS.count()));
}
