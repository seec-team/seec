//===- tools/seec-trace-view/WelcomeFrame.cpp -----------------------------===//
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
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>

#if !wxUSE_WEBVIEW_WEBKIT && !wxUSE_WEBVIEW_IE
  #error "wxWebView backend required!"
#endif

#include <wx/webview.h>
#include <wx/webviewfshandler.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "CommonMenus.hpp"
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
  
  // Setup the webview.
  auto WebView = wxWebView::New(this, wxID_ANY);
  if (!WebView) {
    wxLogDebug("wxWebView::New failed.");
    return false;
  }
  
  WebView->RegisterHandler(wxSharedPtr<wxWebViewHandler>
                                      (new wxWebViewFSHandler("icurb")));

  WebView->LoadURL("icurb://TraceViewer/GUIText/Welcome.html");

  // Make the webview grow to fit the frame.
  auto TopSizer = new wxBoxSizer(wxVERTICAL);
  TopSizer->Add(WebView, wxSizerFlags().Proportion(1).Expand());
  SetSizer(TopSizer);

  return true;
}

void WelcomeFrame::OnClose(wxCommandEvent &Event) {
  Close(true);
}
