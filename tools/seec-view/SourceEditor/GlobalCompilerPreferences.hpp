//===- tools/seec-view/SourceEditor/GlobalCompilerPreferences.hpp ---------===//
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

#ifndef SEEC_VIEW_GLOBALCOMPILERPREFERENCES_HPP
#define SEEC_VIEW_GLOBALCOMPILERPREFERENCES_HPP

#include <wx/filename.h>

#include "../Preferences.hpp"

class wxFilePickerCtrl;

wxFileName getPathForMinGWGCC();

/// \brief Allows the user to configure global compiler preferences.
///
class GlobalCompilerPreferencesWindow final : public PreferenceWindow
{
  wxFilePickerCtrl *m_MinGWGCCPathCtrl;

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
  GlobalCompilerPreferencesWindow();

  /// \brief Constructor (with creation).
  ///
  GlobalCompilerPreferencesWindow(wxWindow *Parent);

  /// \brief Destructor.
  ///
  virtual ~GlobalCompilerPreferencesWindow();

  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent);
};

#endif // SEEC_VIEW_GLOBALCOMPILERPREFERENCES_HPP
