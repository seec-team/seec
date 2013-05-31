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
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Path.h"

#include <cinttypes>
#include <iostream>

#include "SourceViewer.hpp"
#include "StateViewer.hpp"
#include "TraceViewerApp.hpp"
#include "TraceViewerFrame.hpp"


enum ControlIDs {
  TraceViewer_Reset = wxID_HIGHEST,
  TraceViewer_ProcessTime,
  TraceViewer_ThreadTime
};

BEGIN_EVENT_TABLE(TraceViewerFrame, wxFrame)
  EVT_MENU(wxID_CLOSE, TraceViewerFrame::OnClose)

  SEEC_EVT_THREAD_MOVE(TraceViewer_ThreadTime,
                       TraceViewerFrame::OnThreadTimeMove)
END_EVENT_TABLE()

TraceViewerFrame::TraceViewerFrame()
: Trace(),
  State(),
  StateAccess(),
  SourceViewer(nullptr),
  StateViewer(nullptr)
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
  SourceViewer(nullptr),
  StateViewer(nullptr)
{
  Create(Parent, std::move(TracePtr), ID, Title, Position, Size);
}

TraceViewerFrame::~TraceViewerFrame() {
  // Notify the TraceViewerApp that we have been destroyed.
  auto &App = wxGetApp();
  App.removeTopLevelFrame(this);
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

  // Set the trace.
  Trace = std::move(TracePtr);
  
  // Create a new state at the beginning of the trace.
  State = seec::makeUnique<seec::cm::ProcessState>(Trace->getTrace());
  
  // Create a new accessor token for this state.
  StateAccess = std::make_shared<StateAccessToken>();

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Setup the menus.
  auto menuFile = new wxMenu();
  menuFile->Append(wxID_OPEN);
  menuFile->Append(wxID_CLOSE);
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

  auto menuBar = new wxMenuBar();
  menuBar->Append(menuFile,
                  seec::getwxStringExOrEmpty(TextTable, "Menu_File"));

  SetMenuBar(menuBar);

  // Setup a status bar.
  CreateStatusBar();

  if (State->getThreadCount() == 1) {
    // Setup the view for a single-threaded trace.

    // Create the thread time movement control.
    ThreadTime = new ThreadTimeControl(this, TraceViewer_ThreadTime);

    // Create the source code viewer.
    SourceViewer = new SourceViewerPanel(this,
                                         *Trace,
                                         wxID_ANY,
                                         wxDefaultPosition,
                                         wxDefaultSize);

    // Create a text control to show the current state.
    StateViewer = new StateViewerPanel(this,
                                       wxID_ANY,
                                       wxDefaultPosition,
                                       wxDefaultSize);

    wxBoxSizer *TopSizer = new wxBoxSizer(wxVERTICAL);
    TopSizer->Add(ThreadTime, wxSizerFlags().Expand());

    wxBoxSizer *ViewSizer = new wxBoxSizer(wxHORIZONTAL);
    ViewSizer->Add(SourceViewer, wxSizerFlags().Proportion(1).Expand());
    ViewSizer->Add(StateViewer, wxSizerFlags().Proportion(1).Expand());

    TopSizer->Add(ViewSizer, wxSizerFlags().Proportion(1).Expand());

    SetSizer(TopSizer);

    // Display the initial state.
    // StateViewer->show(StateAccess, *State);
    SourceViewer->show(StateAccess, *State, State->getThread(0));
    ThreadTime->show(StateAccess, *State, State->getThread(0), 0);
  }
  else {
    // TODO: Setup the view for a multi-threaded trace.
  }

  return true;
}

void TraceViewerFrame::OnClose(wxCommandEvent &Event) {
  Close(true);
}

void TraceViewerFrame::OnThreadTimeMove(ThreadMoveEvent &Event) {
  // Deny access to the state.
  if (StateAccess)
    StateAccess->invalidate();
  
  // Move the thread.
  auto const Index = Event.getThreadIndex();
  auto &Thread = State->getThread(Index);
  
  switch (Event.getDirection()) {
    case ThreadMoveEvent::DirectionTy::Backward:
      wxLogDebug("Moving backward.");
      seec::cm::moveBackward(Thread);
      break;
    
    case ThreadMoveEvent::DirectionTy::Forward:
      wxLogDebug("Moving forward.");
      seec::cm::moveForward(Thread);
      break;
  }
  
  // Create a new access token for the state.
  StateAccess = std::make_shared<StateAccessToken>();
  
  // Display the new state.
  StateViewer->show(StateAccess, *State, State->getThread(Index));
  SourceViewer->show(StateAccess, *State, State->getThread(Index));
  ThreadTime->show(StateAccess, *State, State->getThread(Index), Index);
}
