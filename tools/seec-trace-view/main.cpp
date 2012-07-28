//===- main.cpp - SeeC Trace Viewer ---------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "TraceViewerFrame.hpp"

#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/Path.h"

#include <unicode/resbund.h>

#include <wx/wx.h>
#include <wx/stdpaths.h>

#include <memory>


//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

class TraceViewerApp : public wxApp
{
  /// \name Interface to wxApp.
  /// @{

  virtual bool OnInit();

  /// @}


  /// \name TraceViewer specific.
  /// @{

  std::unique_ptr<seec::ResourceLoader> ICUResources;

  void HandleFatalError(wxString Description);

  /// @}
};


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

  // Setup the main Frame.
  auto TitleStr = seec::getwxStringEx(TextTable, "FrameTitle", Status);
  if (U_FAILURE(Status))
    HandleFatalError("Couldn't load FrameTitle from GUIText resource bundle!");

  auto Frame = new TraceViewerFrame(TitleStr, wxDefaultPosition, wxDefaultSize);

  Frame->Show(true);

  SetTopWindow(Frame);

  return true;
}

void TraceViewerApp::HandleFatalError(wxString Description) {
  // TODO: If any frames exist, destroy them?

  // Show an error dialog for the user.
  auto ErrorDialog = new wxMessageDialog(NULL,
                                         Description,
                                         "Fatal error!",
                                         wxOK,
                                         wxDefaultPosition);

  ErrorDialog->ShowModal();

  exit(EXIT_FAILURE);
}
