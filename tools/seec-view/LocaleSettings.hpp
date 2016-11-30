//===- tools/seec-trace-view/LocaleSettings.hpp ---------------------------===//
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

#ifndef SEEC_TRACE_VIEW_LOCALESETTINGS_HPP
#define SEEC_TRACE_VIEW_LOCALESETTINGS_HPP

#include <unicode/locid.h>

#include <vector>

#include "Preferences.hpp"

class wxBitmapComboBox;

/// \brief Allows the user to configure locale settings.
///
class LocaleSettingsWindow final : public PreferenceWindow
{
  /// Allows the user to pick from the available locales.
  wxBitmapComboBox *m_Selector;

  /// Stores all available locales in the same order as Selector.
  std::vector<Locale> m_AvailableLocales;

protected:
  /// \brief Save edited values back to the user's config file.
  ///
  virtual bool SaveValuesImpl() override;

  /// \brief Cancel any changes made to the user's settings.
  ///
  virtual void CancelChangesImpl() override;

  /// \bried Get a string to describe this window.
  ///
  virtual wxString GetDisplayNameImpl() override;

public:
  /// \brief Constructor (without creation).
  ///
  LocaleSettingsWindow();

  /// \brief Constructor (with creation).
  ///
  LocaleSettingsWindow(wxWindow *Parent);

  /// \brief Destructor.
  ///
  virtual ~LocaleSettingsWindow();

  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent);
};

/// \brief Get the \c Locale that should be used.
///
icu::Locale getLocale();

#endif // SEEC_TRACE_VIEW_LOCALESETTINGS_HPP
