//===- tools/seec-trace-view/ExplanationViewer.hpp ------------------------===//
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

#ifndef SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP
#define SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP

#include "seec/ClangEPV/ClangEPV.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/stc/stc.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>


// Forward declarations.

namespace clang {
  class Decl;
  class Stmt;
} // namespace clang


/// \brief ExplanationViewer.
///
class ExplanationViewer : public wxStyledTextCtrl
{
  /// Hold current explanatory material.
  std::unique_ptr<seec::clang_epv::Explanation> Explanation;
  
  /// Caches the current mouse position.
  int CurrentMousePosition;
  
  /// \brief Set the contents of this viewer.
  void SetText(wxString const &Value);
  
public:
  /// \brief Construct without creating.
  ExplanationViewer()
  : wxStyledTextCtrl(),
    Explanation(),
    CurrentMousePosition(wxSTC_INVALID_POSITION)
  {}

  /// \brief Construct and create.
  ExplanationViewer(wxWindow *Parent,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : wxStyledTextCtrl(),
    Explanation(),
    CurrentMousePosition(wxSTC_INVALID_POSITION)
  {
    Create(Parent, ID, Position, Size);
  }

  /// \brief Destructor.
  virtual ~ExplanationViewer();

  /// \brief Create the viewer.
  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  
  /// \name Mouse events.
  /// @{
  
  void OnMotion(wxMouseEvent &Event);
  
  void OnEnterWindow(wxMouseEvent &Event);
  
  void OnLeaveWindow(wxMouseEvent &Event);
  
  /// @} (Mouse events)
  
  
  /// \name Mutators.
  /// @{
  
  /// \brief Attempt to show an explanation for the given Decl.
  void showExplanation(::clang::Decl const *Decl);
  
  /// \brief Attempt to show an explanation for the given Stmt.
  void showExplanation(::clang::Stmt const *Statement);
  
  /// \brief Clear the display.
  void clearExplanation();
  
  /// @} (Mutators)
};

#endif // SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP
