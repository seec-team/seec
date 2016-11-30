//===- tools/seec-trace-view/StateGraphViewerPreferences.hpp --------------===//
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

#ifndef SEEC_TRACE_VIEW_STATEGRAPHVIEWERPREFERENCES_HPP
#define SEEC_TRACE_VIEW_STATEGRAPHVIEWERPREFERENCES_HPP

#include "Preferences.hpp"

class wxFilePickerCtrl;

std::string getPathForDotExecutable();

/// \brief Allows the user to configure graph viewer preferences.
///
class StateGraphViewerPreferencesWindow final : public PreferenceWindow
{
  wxFilePickerCtrl *m_DotFilePicker;

protected:
  /// \brief Save edited values back to the user's config file.
  ///
  virtual bool SaveValuesImpl() override;

  /// \brief Cancel any changes made to the user's settings.
  ///
  virtual void CancelChangesImpl() override;

  /// \brief Get a string to describe this window.
  ///
  virtual wxString GetDisplayNameImpl() override;

public:
  /// \brief Constructor (without creation).
  ///
  StateGraphViewerPreferencesWindow();

  /// \brief Constructor (with creation).
  ///
  StateGraphViewerPreferencesWindow(wxWindow *Parent);

  /// \brief Destructor.
  ///
  virtual ~StateGraphViewerPreferencesWindow();

  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent);
};

#endif // SEEC_TRACE_VIEW_STATEGRAPHVIEWERPREFERENCES_HPP
