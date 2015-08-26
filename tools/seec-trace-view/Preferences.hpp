//===- tools/seec-trace-view/Preferences.hpp ------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_PREFERENCES_HPP
#define SEEC_TRACE_VIEW_PREFERENCES_HPP

#include <wx/panel.h>

class PreferenceWindow : public wxPanel
{
protected:
  virtual bool SaveValuesImpl() = 0;

  virtual void CancelChangesImpl() = 0;

  virtual wxString GetDisplayNameImpl() = 0;

public:
  bool SaveValues() { return SaveValuesImpl(); }

  void CancelChanges() { CancelChangesImpl(); }

  wxString GetDisplayName() { return GetDisplayNameImpl(); }
};

void showPreferenceDialog();

#endif // SEEC_TRACE_VIEW_PREFERENCES_HPP
