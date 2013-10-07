//===- tools/seec-trace-view/ActionRecordSettings.hpp ---------------------===//
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

#ifndef SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP
#define SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

/// \brief Shows settings for user action recording.
///
class ActionRecordSettingsFrame : public wxFrame
{
public:
  /// \brief Constructor (without creation).
  ///
  ActionRecordSettingsFrame();
  
  /// \brief Constructor (with creation).
  ///
  ActionRecordSettingsFrame(wxWindow *Parent);
  
  /// \brief Destructor.
  ///
  ~ActionRecordSettingsFrame();
  
  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent);
};

#endif // SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP
