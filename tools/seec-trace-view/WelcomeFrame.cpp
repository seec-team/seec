//===- WelcomeFrame.cpp ---------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/html/htmlwin.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "TraceViewerApp.hpp"
#include "WelcomeFrame.hpp"

// Define the event table for WelcomeFrame.
BEGIN_EVENT_TABLE(WelcomeFrame, wxFrame)
  EVT_MENU(wxID_CLOSE, WelcomeFrame::OnClose)
END_EVENT_TABLE()

WelcomeFrame::~WelcomeFrame() {
  auto &App = wxGetApp();
  App.removeTopLevelFrame(this);
}

bool WelcomeFrame::Create(wxWindow *Parent,
                          wxWindowID ID,
                          wxString const &Title,
                          wxPoint const &Position,
                          wxSize const &Size)
{
  if (!wxFrame::Create(Parent, ID, Title, Position, Size))
    return false;

#if 0
  auto LogWindow = new wxLogWindow(this, wxString("Log"));
  LogWindow->Show();
#endif

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

  // Create a HTML window to view the welcome message.
  auto HTMLWindow = new wxHtmlWindow(this);
  HTMLWindow->SetPage(seec::getwxStringExOrEmpty(TextTable, "Welcome_Message"));

  // Make the HTML window grow to fit the frame.
  auto TopSizer = new wxBoxSizer(wxVERTICAL);
  TopSizer->Add(HTMLWindow, wxSizerFlags().Proportion(1).Expand());
  SetSizer(TopSizer);

  return true;
}

void WelcomeFrame::OnClose(wxCommandEvent &Event) {
  Close(true);
}
