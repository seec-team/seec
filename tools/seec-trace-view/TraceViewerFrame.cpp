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

#include "llvm/Support/raw_os_ostream.h"

#include <iostream>

enum ControlIDs {
  TraceViewer_Reset = wxID_HIGHEST,
  TraceViewer_SlideProcessTime
};

BEGIN_EVENT_TABLE(TraceViewerFrame, wxFrame)
#define SEEC_COMMAND_EVENT(EVENT) \
  EVT_MENU(ID_##EVENT, TraceViewerFrame::On##EVENT)
#include "TraceViewerFrameEvents.def"

  EVT_COMMAND_SCROLL_CHANGED(TraceViewer_SlideProcessTime,
                             TraceViewerFrame::OnSlideProcessTimeChanged)
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
  SetMinSize(wxSize(400,300));

  // Create a slider to control the current process time.
  SlideProcessTime = new wxSlider(this,
                                  TraceViewer_SlideProcessTime,
                                  0, // Value
                                  0, // MinValue
                                  0, // MaxValue
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  wxSL_HORIZONTAL | wxSL_LABELS);

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

  // Add all the child controls using the AUI manager.
  auto Caption = seec::getwxStringExOrDie(TextTable, "ScrollProcessTime_Title");
  SlideProcessTime->SetLabel(Caption);
  SlideProcessTime->Enable(false); // Disable the slider.
  Manager.AddPane(SlideProcessTime,
                  wxAuiPaneInfo().Top()
                                 .CloseButton(false)
                                 .Caption(Caption)
                                 .CaptionVisible(false)
                                 .Resizable(false)
                                 .Floatable(false));

  Caption = seec::getwxStringExOrDie(TextTable, "SourceBook_Title");
  Manager.AddPane(SourceViewer,
                  wxAuiPaneInfo().Left()
                                 .CloseButton(false)
                                 .Caption(Caption)
                                 .CaptionVisible(true)
                                 .Resizable(true)
                                 .Floatable(false));

  Caption = seec::getwxStringExOrDie(TextTable, "StatusView_Title");
  Manager.AddPane(StateViewer,
                  wxAuiPaneInfo().CenterPane()
                                 .PaneBorder(false)
                                 .CloseButton(false)
                                 .Caption(Caption)
                                 .CaptionVisible(true)
                                 .Resizable(true)
                                 .Floatable(false));

  Manager.Update();

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
    SlideProcessTime->SetRange(0, FinalProcessTime);
    SlideProcessTime->SetValue(0);
    SlideProcessTime->Enable(true); // Enable the slider.

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

void TraceViewerFrame::OnSlideProcessTimeChanged(wxScrollEvent& event) {
  State->setProcessTime(event.GetPosition());
  StateViewer->show(*Trace, *State);
}
