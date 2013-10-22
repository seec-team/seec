//===- tools/seec-trace-view/TraceViewerApp.hpp ---------------------------===//
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

#ifndef SEEC_TRACE_VIEW_TRACEVIEWERAPP_HPP
#define SEEC_TRACE_VIEW_TRACEVIEWERAPP_HPP

#include "seec/ICU/Resources.hpp"

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>
#include <set>
#include <vector>

//------------------------------------------------------------------------------
// TraceViewerApp
//------------------------------------------------------------------------------

class WelcomeFrame;

/// \brief The application class for the SeeC Trace Viewer.
///
class TraceViewerApp : public wxApp
{
  /// The welcome frame that is displayed when no files are open.
  WelcomeFrame *Welcome;

  /// All other top-level windows.
  std::set<wxWindow *> TopLevelWindows;

  /// The log window.
  wxLogWindow *LogWindow;

  /// Holds the ICU resource files used by this application.
  std::unique_ptr<seec::ResourceLoader> ICUResources;
  
  /// Files that the user passed on the command line.
  std::vector<wxString> CLFiles;

  /// \brief Open a new trace viewer for the given file.
  void OpenFile(wxString const &FileName);

public:
  /// \brief Constructor.
  TraceViewerApp()
  : wxApp(),
    Welcome(nullptr),
    TopLevelWindows(),
    LogWindow(nullptr),
    ICUResources(),
    CLFiles()
  {}

  /// \name Interface to wxApp.
  /// @{

  virtual bool OnInit();
  
  virtual void OnInitCmdLine(wxCmdLineParser &Parser);
  
  virtual bool OnCmdLineParsed(wxCmdLineParser &Parser);

  /// @}


  /// \name Mac OS X functionality
  /// @{

  virtual void MacNewFile();

  virtual void MacOpenFiles(wxArrayString const &FileNames);

  virtual void MacOpenFile(wxString const &FileName);

  virtual void MacReopenApp();

  /// @}


  /// \name Handle application-wide events.
  /// @{

  /// \brief Allow the user to open a file.
  /// At this time, the only files supported are pre-recorded SeeC traces,
  /// which are selected by opening the SeeC Process Trace (.spt) file.
  void OnCommandOpen(wxCommandEvent &Event);

  /// \brief Quit the application.
  void OnCommandExit(wxCommandEvent &Event);

  /// @}


  /// \name TraceViewer specific.
  /// @{

  void HandleFatalError(wxString Description);
  
  /// \brief Notify that a top-level window is being added.
  void addTopLevelWindow(wxWindow *Window) {
    TopLevelWindows.insert(Window);
  }

  /// \brief Notify that the welcome window is being destroyed.
  void removeTopLevelWindow(WelcomeFrame *Window) {
    if (!Welcome)
      return;

    assert(Welcome == Window);
    Welcome = nullptr;
  }

  /// \brief Notify that a top-level window is being destroyed.
  void removeTopLevelWindow(wxWindow *Window) {
    TopLevelWindows.erase(Window);
  }

  /// @}

private:
  DECLARE_EVENT_TABLE()
};

DECLARE_APP(TraceViewerApp)

#endif // SEEC_TRACE_VIEW_TRACEVIEWERAPP_HPP
