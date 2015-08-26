//===- tools/seec-trace-view/TracingPreferences.hpp -----------------------===//
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

#ifndef SEEC_TRACE_VIEW_TRACINGPREFERENCES_HPP
#define SEEC_TRACE_VIEW_TRACINGPREFERENCES_HPP

#include "Preferences.hpp"

/// \brief Allows the user to configure tracing preferences.
///
class TracingPreferencesWindow final : public PreferenceWindow
{
  wxSlider *m_ThreadEventLimit;

  wxSlider *m_ArchiveLimit;

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
  TracingPreferencesWindow();

  /// \brief Constructor (with creation).
  ///
  TracingPreferencesWindow(wxWindow *Parent);

  /// \brief Destructor.
  ///
  virtual ~TracingPreferencesWindow();

  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent);
};

#endif // SEEC_TRACE_VIEW_TRACINGPREFERENCES_HPP
