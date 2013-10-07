//===- tools/seec-trace-view/ActionRecordSettings.cpp ---------------------===//
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

#include "seec/wxWidgets/StringConversion.hpp"

#include "ActionRecordSettings.hpp"
#include "TraceViewerApp.hpp"

ActionRecordSettingsFrame::ActionRecordSettingsFrame()
{}

ActionRecordSettingsFrame::ActionRecordSettingsFrame(wxWindow *Parent)
{
  Create(Parent);
}

ActionRecordSettingsFrame::~ActionRecordSettingsFrame()
{
  // Notify the TraceViewerApp that we no longer exist.
  auto &App = wxGetApp();
  App.removeTopLevelFrame(this);
}

bool ActionRecordSettingsFrame::Create(wxWindow *Parent)
{
  // Get the title.
  auto const Title =
    seec::getwxStringExOrEmpty("TraceViewer",
      (char const * []){"GUIText", "RecordingSettingsDialog", "Title"});
  
  if (!wxFrame::Create(Parent,
                       wxID_ANY,
                       Title,
                       wxDefaultPosition,
                       wxDefaultSize))
    return false;
  
  // Notify the TraceViewerApp that we exist.
  auto &App = wxGetApp();
  App.addTopLevelFrame(this);
  
  // Create the token input.
  
  // Create the size limit slider.
  
  // Create accept/cancel buttons.
  
  return true;
}
