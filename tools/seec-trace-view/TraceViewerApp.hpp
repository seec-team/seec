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

  /// All other top-level frames.
  std::set<wxFrame *> TopLevelFrames;

  /// The log window.
  wxLogWindow *LogWindow;

  /// Holds the ICU resource files used by this application.
  std::unique_ptr<seec::ResourceLoader> ICUResources;

  /// \brief Open a new trace viewer for the given file.
  void OpenFile(wxString const &FileName);

public:
  /// \brief Constructor.
  TraceViewerApp()
  : wxApp(),
    Welcome(nullptr),
    TopLevelFrames(),
    LogWindow(nullptr),
    ICUResources()
  {}

  /// \name Interface to wxApp.
  /// @{

  virtual bool OnInit();

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

  /// \brief Notify that the welcome frame is being destroyed.
  void removeTopLevelFrame(WelcomeFrame *Frame) {
    if (!Welcome)
      return;

    assert(Welcome == Frame);
    Welcome = nullptr;
  }

  /// \brief Notify that a top-level frame is being destroyed.
  void removeTopLevelFrame(wxFrame *Frame) {
    TopLevelFrames.erase(Frame);
  }

  /// @}

private:
  DECLARE_EVENT_TABLE()
};

DECLARE_APP(TraceViewerApp)

#endif // SEEC_TRACE_VIEW_TRACEVIEWERAPP_HPP
