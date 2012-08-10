//===- TraceViewerFrame.cpp -----------------------------------------------===//
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
#include "seec/Util/Range.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Path.h"

#include <iostream>

#include "TraceViewerApp.hpp"
#include "TraceViewerFrame.hpp"

enum ControlIDs {
  TraceViewer_Reset = wxID_HIGHEST,
  TraceViewer_ProcessTime
};

BEGIN_EVENT_TABLE(TraceViewerFrame, wxFrame)
  EVT_MENU(wxID_CLOSE, TraceViewerFrame::OnClose)

  SEEC_EVT_PROCESS_TIME_CHANGED(TraceViewer_ProcessTime,
                                TraceViewerFrame::OnProcessTimeChanged)
END_EVENT_TABLE()

TraceViewerFrame::~TraceViewerFrame() {
  auto &App = wxGetApp();
  App.removeTopLevelFrame(this);
}

bool TraceViewerFrame::Create(wxWindow *Parent,
                              std::unique_ptr<OpenTrace> &&TracePtr,
                              wxWindowID ID,
                              wxString const &Title,
                              wxPoint const &Position,
                              wxSize const &Size)
{
  if (!wxFrame::Create(Parent, ID, Title, Position, Size))
    return false;

  // Set the trace.
  Trace = std::move(TracePtr);

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

  // Create a control for the current process time.
  ProcessTime = new ProcessTimeControl(this,
                                       TraceViewer_ProcessTime);

  // Create the source code viewer.
  SourceViewer = new SourceViewerPanel(this,
                                       wxID_ANY,
                                       wxDefaultPosition,
                                       wxDefaultSize);

  // Create a text control to show the current state.
  StateViewer = new StateViewerPanel(this,
                                     wxID_ANY,
                                     wxDefaultPosition,
                                     wxDefaultSize);

  wxBoxSizer *TopSizer = new wxBoxSizer(wxVERTICAL);
  TopSizer->Add(ProcessTime, wxSizerFlags().Expand());

  wxBoxSizer *ViewSizer = new wxBoxSizer(wxHORIZONTAL);
  ViewSizer->Add(SourceViewer, wxSizerFlags().Proportion(1).Expand());
  ViewSizer->Add(StateViewer, wxSizerFlags().Proportion(2).Expand());

  TopSizer->Add(ViewSizer, wxSizerFlags().Proportion(1).Expand());

  SetSizer(TopSizer);

  // Temporary.
  // Create a new ProcessState at the beginning of the trace.
  State.reset(new seec::trace::ProcessState(Trace->getProcessTrace(),
                                            Trace->getModuleIndex()));

  // Show a message indicating that the new trace was loaded.
  int64_t FinalProcessTime = Trace->getProcessTrace().getFinalProcessTime();
  auto StatusMsg = TextTable.getStringEx("OpenTrace_Status_Loaded", Status);
  auto Formatted = seec::format(StatusMsg, Status, FinalProcessTime);
  SetStatusText(seec::towxString(Formatted));

  // Display information about the newly-read trace.
  ProcessTime->setTrace(*Trace);
  StateViewer->show(*Trace, *State);
  SourceViewer->show(*Trace, *State);

  return true;
}

void TraceViewerFrame::OnClose(wxCommandEvent &Event) {
  Close(true);
}

void TraceViewerFrame::OnProcessTimeChanged(ProcessTimeEvent& Event) {
  State->setProcessTime(Event.getProcessTime());
  StateViewer->show(*Trace, *State);
  SourceViewer->show(*Trace, *State);
}
