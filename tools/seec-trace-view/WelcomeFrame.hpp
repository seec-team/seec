//===- tools/seec-trace-view/WelcomeFrame.hpp -----------------------------===//
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

#ifndef SEEC_TRACE_VIEW_WELCOMEFRAME_HPP
#define SEEC_TRACE_VIEW_WELCOMEFRAME_HPP

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

/// \brief Frame to display when no files are open.
///
class WelcomeFrame : public wxFrame
{
public:
  /// \brief Construct without creating.
  WelcomeFrame()
  : wxFrame()
  {}

  /// \brief Construct and create.
  WelcomeFrame(wxWindow *Parent,
               wxWindowID ID = wxID_ANY,
               wxString const &Title = wxString(),
               wxPoint const &Position = wxDefaultPosition,
               wxSize const &Size = wxDefaultSize)
  : wxFrame()
  {
    Create(Parent, ID, Title, Position, Size);
  }

  /// \brief Destructor.
  ~WelcomeFrame();

  /// \brief Create.
  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxString const &Title = wxString(),
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Close the current file.
  void OnClose(wxCommandEvent &Event);

private:
  DECLARE_EVENT_TABLE()
};

#endif // SEEC_TRACE_VIEW_WELCOMEFRAME_HPP
