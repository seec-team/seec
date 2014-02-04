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

#include "seec/Clang/MappedValue.hpp"
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

namespace seec {
  namespace cm {
    class FunctionState;
    class ProcessState;
  }
}

class ContextNotifier;
class StateAccessToken;


/// \brief ExplanationViewer.
///
class ExplanationViewer final : public wxStyledTextCtrl
{
  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// Hold current explanatory material.
  std::unique_ptr<seec::clang_epv::Explanation> Explanation;
  
  /// Caches the current mouse position.
  int CurrentMousePosition;
  
  /// Currently highlighted Decl.
  ::clang::Decl const *HighlightedDecl;
  
  /// Currently highlighted Stmt.
  ::clang::Stmt const *HighlightedStmt;
  
  /// Is the mouse currently hovering on a URL?
  bool URLHover;
  
  /// Is the mouse on the same URL as when the left button was clicked?
  bool URLClick;
  
  /// \brief Set the contents of this viewer.
  ///
  void setText(wxString const &Value);
  
  /// \brief Clear the current information.
  ///
  void clearCurrent();
  
public:
  /// \brief Construct without creating.
  ///
  ExplanationViewer()
  : wxStyledTextCtrl(),
    Notifier(nullptr),
    Explanation(),
    CurrentMousePosition(wxSTC_INVALID_POSITION),
    HighlightedDecl(nullptr),
    HighlightedStmt(nullptr),
    URLHover(false),
    URLClick(false)
  {}

  /// \brief Construct and create.
  ///
  ExplanationViewer(wxWindow *Parent,
                    ContextNotifier &WithNotifier,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : ExplanationViewer()
  {
    Create(Parent, WithNotifier, ID, Position, Size);
  }

  /// \brief Destructor.
  ///
  virtual ~ExplanationViewer();

  /// \brief Create the viewer.
  ///
  bool Create(wxWindow *Parent,
              ContextNotifier &WithNotifier,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  
  /// \name Mouse events.
  /// @{
  
  void OnMotion(wxMouseEvent &Event);
  
  void OnEnterWindow(wxMouseEvent &Event);
  
  void OnLeaveWindow(wxMouseEvent &Event);
  
  void OnLeftDown(wxMouseEvent &Event);
  
  void OnLeftUp(wxMouseEvent &Event);
  
  /// @} (Mouse events)
  
  
  /// \name Mutators.
  /// @{
  
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);
  
  /// \brief Attempt to show an explanation for the given Decl.
  ///
  void showExplanation(::clang::Decl const *Decl);
  
  /// \brief Attempt to show an explanation for the given Stmt.
  ///
  /// pre: Caller must have locked access to the state containing InFunction.
  ///
  void showExplanation(::clang::Stmt const *Statement,
                       ::seec::cm::FunctionState const &InFunction);
  
  /// \brief Clear the display.
  ///
  void clearExplanation();
  
  /// @} (Mutators)
};

#endif // SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP
