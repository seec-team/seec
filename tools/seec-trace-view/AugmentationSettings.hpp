//===- tools/seec-trace-view/AugmentationSettings.hpp ---------------------===//
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

#ifndef SEEC_TRACE_VIEW_AUGMENTATIONSETTINGS_HPP
#define SEEC_TRACE_VIEW_AUGMENTATIONSETTINGS_HPP

#include "Preferences.hpp"

class wxCommandEvent;

/// \brief Allows the user to configure augmentations.
///
class AugmentationSettingsWindow final : public PreferenceWindow
{
private:
  void OnDownloadClick(wxCommandEvent &Ev);

protected:
  /// \brief Save edited values back to the user's config file.
  ///
  virtual bool SaveValuesImpl() override;

  /// \bried Get a string to describe this window.
  ///
  virtual wxString GetDisplayNameImpl() override;

public:
  /// \brief Constructor (without creation).
  ///
  AugmentationSettingsWindow();

  /// \brief Constructor (with creation).
  ///
  AugmentationSettingsWindow(wxWindow *Parent);

  /// \brief Destructor.
  ///
  virtual ~AugmentationSettingsWindow() override;

  /// \brief Create the frame.
  ///
  bool Create(wxWindow *Parent);
};

#endif // SEEC_TRACE_VIEW_AUGMENTATIONSETTINGS_HPP
