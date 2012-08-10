//===- TraceViewerApp.hpp -------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
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

class TraceViewerApp : public wxApp
{
private:
  /// The welcome frame that is displayed when no files are open.
  WelcomeFrame *Welcome;

  /// All other top-level frames.
  std::set<wxFrame *> TopLevelFrames;

  /// Holds the ICU resource files used by this application.
  std::unique_ptr<seec::ResourceLoader> ICUResources;

public:
  /// \brief Constructor.
  TraceViewerApp()
  : wxApp(),
    TopLevelFrames()
  {}

  /// \name Interface to wxApp.
  /// @{

  virtual bool OnInit();

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
