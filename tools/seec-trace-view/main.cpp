//===- main.cpp - SeeC Trace Viewer ---------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/ICU/Resources.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"

#include <unicode/resbund.h>

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>
#include <set>

#include "OpenTrace.hpp"
#include "TraceViewerApp.hpp"
#include "TraceViewerFrame.hpp"
#include "WelcomeFrame.hpp"


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

// Define the event table for TraceViewerApp.
BEGIN_EVENT_TABLE(TraceViewerApp, wxApp)
  EVT_MENU(wxID_OPEN, TraceViewerApp::OnCommandOpen)
  EVT_MENU(wxID_EXIT, TraceViewerApp::OnCommandExit)
END_EVENT_TABLE()



//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

IMPLEMENT_APP(TraceViewerApp)


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

bool TraceViewerApp::OnInit() {
  // Find the path to the executable.
  wxStandardPaths StdPaths;
  char const *ExecutablePath = StdPaths.GetExecutablePath().c_str();

  // Load ICU resources for TraceViewer.
  ICUResources.reset(new seec::ResourceLoader(llvm::sys::Path{ExecutablePath}));
  if (!ICUResources->loadResource("TraceViewer"))
    HandleFatalError("Couldn't load TraceViewer resources!");

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  if (U_FAILURE(Status))
    HandleFatalError("Couldn't load resource bundle TraceViewer->GUIText!");

  // Setup OS-X behaviour.
#ifdef __WXMAC__
  wxApp::SetExitOnFrameDelete(false);

  // Setup common menus.
  auto menuFile = new wxMenu();
  menuFile->Append(wxID_OPEN);
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

  auto menuBar = new wxMenuBar();
  menuBar->Append(menuFile,
                  seec::getwxStringExOrEmpty(TextTable, "Menu_File"));

  wxMenuBar::MacSetCommonMenuBar(menuBar);
#endif

  // Setup the welcome frame.
  Welcome = new WelcomeFrame(nullptr,
                             wxID_ANY,
                             seec::getwxStringExOrEmpty(TextTable,
                                                        "Welcome_Title"),
                             wxDefaultPosition,
                             wxDefaultSize);
  Welcome->Show(true);

  return true;
}

void TraceViewerApp::OnCommandOpen(wxCommandEvent &WXUNUSED(Event)) {
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  // Create the open file dialog.
  wxFileDialog *OpenDialog
    = new wxFileDialog(nullptr,
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

  // Show the dialog and exit if the user didn't select a file.
  if (OpenDialog->ShowModal() != wxID_OK)
    return;

  // Attempt to read the trace, which should either return the newly read trace
  // (in Maybe slot 0), or an error message (in Maybe slot 1).
  auto NewTrace = OpenTrace::FromFilePath(OpenDialog->GetPath());
  assert(NewTrace.assigned());

  if (NewTrace.assigned(0)) {
    // The trace was read successfully, so create a new viewer to display it.
    auto TraceViewer = new TraceViewerFrame(nullptr,
                                            std::move(NewTrace.get<0>()));
    TopLevelFrames.insert(TraceViewer);
    TraceViewer->Show(true);

    // Hide the Welcome frame (on Mac OS X), or destroy it (all others).
#ifdef __WXMAC__
    if (Welcome)
      Welcome->Show(false);
#else
    if (Welcome)
      Welcome->Close(true);
#endif
  }
  else if (NewTrace.assigned(1)) {
    // Display the error that occured.
    auto ErrorDialog = new wxMessageDialog(nullptr, NewTrace.get<1>());
    ErrorDialog->ShowModal();
    ErrorDialog->Destroy();
  }
}

void TraceViewerApp::OnCommandExit(wxCommandEvent &WXUNUSED(Event)) {
#ifdef __WXMAC_
  wxApp::SetExitOnFrameDelete(true);
#endif

  for (auto Frame : TopLevelFrames) {
    Frame->Close(true);
  }
}

void TraceViewerApp::HandleFatalError(wxString Description) {
  // Show an error dialog for the user.
  auto ErrorDialog = new wxMessageDialog(NULL,
                                         Description,
                                         "Fatal error!",
                                         wxOK,
                                         wxDefaultPosition);
  ErrorDialog->ShowModal();

  exit(EXIT_FAILURE);
}
