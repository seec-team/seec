//===- TraceViewerFrame.cpp -----------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "TraceViewerFrame.hpp"

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/Range.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Path.h"

#include <iostream>

enum ControlIDs {
  TraceViewer_Reset = wxID_HIGHEST,
  TraceViewer_ProcessTime
};

BEGIN_EVENT_TABLE(TraceViewerFrame, wxFrame)
#define SEEC_COMMAND_EVENT(EVENT) \
  EVT_MENU(ID_##EVENT, TraceViewerFrame::On##EVENT)
#include "TraceViewerFrameEvents.def"

  SEEC_EVT_PROCESS_TIME_CHANGED(TraceViewer_ProcessTime,
                                TraceViewerFrame::OnProcessTimeChanged)
END_EVENT_TABLE()

bool TraceViewerFrame::Create(wxWindow *Parent,
                              wxWindowID ID,
                              wxString const &Title,
                              wxPoint const &Position,
                              wxSize const &Size)
{
  if (!wxFrame::Create(Parent, ID, Title, Position, Size))
    return false;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;

  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Setup the menu bar.
  wxMenu *menuFile = new wxMenu();
  menuFile->Append(ID_OpenTrace,
                   seec::getwxStringExOrDie(TextTable, "Menu_File_Open"));
  menuFile->AppendSeparator();
  menuFile->Append(ID_Quit,
                   seec::getwxStringExOrDie(TextTable, "Menu_File_Exit"));

  wxMenuBar *menuBar = new wxMenuBar();
  menuBar->Append(menuFile,
                  seec::getwxStringExOrDie(TextTable, "Menu_File"));

  SetMenuBar(menuBar);

  // Setup a status bar.
  CreateStatusBar();

  // Hardcoded minimum size for the frame.
  SetMinSize(wxSize(640,480));

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
  ViewSizer->Add(StateViewer, wxSizerFlags().Proportion(1).Expand());

  TopSizer->Add(ViewSizer, wxSizerFlags().Proportion(1).Expand());

  SetSizer(TopSizer);
  TopSizer->SetSizeHints(this);

  return true;
}

void TraceViewerFrame::OnQuit(wxCommandEvent &WXUNUSED(Event)) {
  Close(true);
}

void TraceViewerFrame::OnOpenTrace(wxCommandEvent &WXUNUSED(Event)) {
  UErrorCode Status = U_ZERO_ERROR;

  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  wxFileDialog *OpenDialog
    = new wxFileDialog(this,
                       seec::getwxStringExOrDie(TextTable,
                                                "OpenTrace_Title"),
                       wxEmptyString,
                       wxEmptyString,
                       seec::getwxStringExOrDie(TextTable,
                                                "OpenTrace_FileType"),
                       wxFD_OPEN,
                       wxDefaultPosition);

  // Destroy the dialog when we leave this scope.
  seec::ScopeExit DestroyDialog([=](){OpenDialog->Destroy();});

  if (OpenDialog->ShowModal() != wxID_OK)
    return;

  // Attempt to read the trace, which should either return the newly read trace
  // (in Maybe slot 0), or an error key (in Maybe slot 1).
  auto NewTrace = OpenTrace::FromFilePath(OpenDialog->GetPath());
  assert(NewTrace.assigned());

  if (NewTrace.assigned(0)) {
    // The trace was read successfully.
    // Clean up the old trace before we change it.
    SourceViewer->clear();

    // Set the newly-read trace.
    Trace = std::move(NewTrace.get<0>());

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

    for (auto &MapGlobalPair : Trace->getMappedModule().getGlobalLookup()) {
      SourceViewer->addSourceFile(MapGlobalPair.second.getFilePath());
    }
  }
  else if (NewTrace.assigned(1)) {
    // Display the error that occured.
    auto ErrorDialog
      = new wxMessageDialog(this,
                            seec::getwxStringExOrDie(TextTable,
                                                     NewTrace.get<1>()));
    ErrorDialog->ShowModal();
    ErrorDialog->Destroy();
  }
}

void TraceViewerFrame::OnProcessTimeChanged(ProcessTimeEvent& Event) {
  State->setProcessTime(Event.getProcessTime());
  StateViewer->show(*Trace, *State);
}
