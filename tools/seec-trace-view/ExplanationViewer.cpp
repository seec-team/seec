//===- tools/seec-trace-view/ExplanationViewer.cpp ------------------------===//
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

#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"

#include "ExplanationViewer.hpp"
#include "HighlightEvent.hpp"
#include "SourceViewerSettings.hpp"

void ExplanationViewer::setText(wxString const &Value)
{
  this->SetEditable(true);
  this->ClearAll();
  this->SetValue(Value);
  this->SetEditable(false);
  this->ClearSelections();
}

void ExplanationViewer::clearCurrent()
{
  IndicatorClearRange(0, GetTextLength());
  
  CurrentMousePosition = wxSTC_INVALID_POSITION;
  
  if (HighlightedDecl) {
    HighlightEvent Ev(SEEC_EV_HIGHLIGHT_OFF, GetId(), HighlightedDecl);
    Ev.SetEventObject(this);
    ProcessWindowEvent(Ev);
    
    HighlightedDecl = nullptr;
  }
  
  if (HighlightedStmt) {
    HighlightEvent Ev(SEEC_EV_HIGHLIGHT_OFF, GetId(), HighlightedStmt);
    Ev.SetEventObject(this);
    ProcessWindowEvent(Ev);
    
    HighlightedStmt = nullptr;
  }
}

ExplanationViewer::~ExplanationViewer() {}

bool ExplanationViewer::Create(wxWindow *Parent,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size)
{
  if (!wxStyledTextCtrl::Create(Parent,
                                ID,
                                Position,
                                Size))
    return false;
  
  Bind(wxEVT_MOTION, &ExplanationViewer::OnMotion, this);
  Bind(wxEVT_ENTER_WINDOW, &ExplanationViewer::OnEnterWindow, this);
  Bind(wxEVT_LEAVE_WINDOW, &ExplanationViewer::OnLeaveWindow, this);
  
  // setupAllSciCommonTypes(*this);
  // setupAllSciLexerTypes(*this);
  setupAllSciIndicatorTypes(*this);
  
  SetEditable(false);
  SetWrapMode(wxSTC_WRAP_WORD);
  
  return true;
}

void ExplanationViewer::OnMotion(wxMouseEvent &Event)
{
  // When we exit this scope, call Event.Skip() so that it is handled by the
  // default handler.
  auto const SkipEvent = seec::scopeExit([&](){ Event.Skip(); });
  
  auto const Pos = CharPositionFromPointClose(Event.GetPosition().x,
                                              Event.GetPosition().y);
  
  if (Pos == CurrentMousePosition)
    return;
  
  clearCurrent();
  CurrentMousePosition = Pos;
  
  if (Pos == wxSTC_INVALID_POSITION)
    return;
  
  auto Links = Explanation->getCharacterLinksAt(Pos);
  if (Links.getPrimaryIndex().isEmpty())
    return;
  
  SetIndicatorCurrent(static_cast<int>(SciIndicatorType::CodeHighlight));
  IndicatorFillRange(Links.getPrimaryIndexStart(),
                     Links.getPrimaryIndexEnd() - Links.getPrimaryIndexStart());
  
  if (auto const Decl = Links.getPrimaryDecl()) {
    HighlightedDecl = Decl;
    HighlightEvent Ev(SEEC_EV_HIGHLIGHT_ON, GetId(), Decl);
    Ev.SetEventObject(this);
    ProcessWindowEvent(Ev);
  }
  
  if (auto const Stmt = Links.getPrimaryStmt()) {
    HighlightedStmt = Stmt;
    HighlightEvent Ev(SEEC_EV_HIGHLIGHT_ON, GetId(), Stmt);
    Ev.SetEventObject(this);
    ProcessWindowEvent(Ev);
  }
}

void ExplanationViewer::OnEnterWindow(wxMouseEvent &Event)
{
}

void ExplanationViewer::OnLeaveWindow(wxMouseEvent &Event)
{
  clearCurrent();
}

void ExplanationViewer::showExplanation(::clang::Decl const *Decl)
{
  auto MaybeExplanation = seec::clang_epv::explain(Decl);
  
  if (MaybeExplanation.assigned(0)) {
    Explanation = std::move(MaybeExplanation.get<0>());
    setText(seec::towxString(Explanation->getString()));
  }
  else if (MaybeExplanation.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto String = MaybeExplanation.get<seec::Error>().getMessage(Status,
                                                                 Locale());
    if (U_SUCCESS(Status)) {
      setText(seec::towxString(String));
    }
    else {
      wxLogDebug("Indescribable error with seec::clang_epv::explain().");
    }
  }
  else if (!MaybeExplanation.assigned()) {
    wxLogDebug("No explanation for Decl %s", Decl->getDeclKindName());
  }
}

void ExplanationViewer::showExplanation(::clang::Stmt const *Statement)
{
  auto MaybeExplanation = seec::clang_epv::explain(Statement);
  if (MaybeExplanation.assigned(0)) {
    Explanation = std::move(MaybeExplanation.get<0>());
    setText(seec::towxString(Explanation->getString()));
  }
  else if (MaybeExplanation.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto String = MaybeExplanation.get<seec::Error>().getMessage(Status,
                                                                 Locale());
    if (U_SUCCESS(Status)) {
      setText(seec::towxString(String));
    }
    else {
      wxLogDebug("Indescribable error with seec::clang_epv::explain().");
    }
  }
  else if (!MaybeExplanation.assigned()) {
    wxLogDebug("No explanation for Stmt %s", Statement->getStmtClassName());
  }
}

void ExplanationViewer::clearExplanation()
{
  // Ensure that highlights etc. are cleared (if they are active).
  clearCurrent();
  
  // Discard the explanation and clear the display.
  Explanation.reset();
  setText(wxEmptyString);
}
