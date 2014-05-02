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

#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"

#include <wx/cursor.h>
#include <wx/utils.h>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "ExplanationViewer.hpp"
#include "NotifyContext.hpp"
#include "SourceViewerSettings.hpp"
#include "StateAccessToken.hpp"


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
  SetCursor(wxCursor(wxCURSOR_ARROW));
  
  if (HighlightedDecl) {
    HighlightedDecl = nullptr;
    
    Notifier->createNotify<ConEvHighlightDecl>(HighlightedDecl);
  }
  
  if (HighlightedStmt) {
    HighlightedStmt = nullptr;
    
    Notifier->createNotify<ConEvHighlightStmt>(HighlightedStmt);
  }
  
  URLHover = false;
}

ExplanationViewer::~ExplanationViewer() {}

bool ExplanationViewer::Create(wxWindow *Parent,
                               ContextNotifier &WithNotifier,
                               ActionRecord &WithRecording,
                               ActionReplayFrame &WithReplay,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size)
{
  if (!wxStyledTextCtrl::Create(Parent,
                                ID,
                                Position,
                                Size))
    return false;
  
  Notifier = &WithNotifier;
  Recording = &WithRecording;
  
  Bind(wxEVT_MOTION, &ExplanationViewer::OnMotion, this);
  Bind(wxEVT_ENTER_WINDOW, &ExplanationViewer::OnEnterWindow, this);
  Bind(wxEVT_LEAVE_WINDOW, &ExplanationViewer::OnLeaveWindow, this);
  Bind(wxEVT_LEFT_DOWN, &ExplanationViewer::OnLeftDown, this);
  Bind(wxEVT_LEFT_UP, &ExplanationViewer::OnLeftUp, this);
  
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
  
  if (Pos == wxSTC_INVALID_POSITION) {
    URLClick = false;
    return;
  }
  
  // This is the "whole character" offset (regardless of the text's encoding).
  auto const Count = CountCharacters(0, Pos);
  
  auto const Links = Explanation->getCharacterLinksAt(Count);
  if (Links.getPrimaryIndex().isEmpty())
    return;
  
  SetIndicatorCurrent(static_cast<int>(SciIndicatorType::CodeHighlight));
  
  // Get the byte offset rather than the "whole character" index.
  auto const StartIndex = Links.getPrimaryIndexStart();
  
  // Initially set the offset to the first valid offset preceding the
  // "whole character" index. This will always be less than the required offset
  // (because no encoding uses less than one byte per character).
  int StartPos = PositionBefore(StartIndex);
  
  // Find the "whole character" index of the guessed position, use that to
  // determine how many characters away from the desired position we are, and
  // then iterate to the desired position.
  auto const StartGuessCount = CountCharacters(0, StartPos);
  for (int i = 0; i < StartIndex - StartGuessCount; ++i)
    StartPos = PositionAfter(StartPos);
  
  // Get the EndPos by iterating from the StartPos.
  auto const Length = Links.getPrimaryIndexEnd() - Links.getPrimaryIndexStart();
  int EndPos = StartPos;
  for (int i = 0; i < Length; ++i)
    EndPos = PositionAfter(EndPos);
  
  IndicatorFillRange(StartPos, EndPos - StartPos);
  
  if (auto const Decl = Links.getPrimaryDecl()) {
    if (HighlightedDecl != Decl) {
      HighlightedDecl = Decl;
      
      Notifier->createNotify<ConEvHighlightDecl>(HighlightedDecl);

      if (Recording) {
        Recording->recordEventL("ExplanationViewer.MouseOverDeclLink",
                                make_attribute("decl", Decl));
      }
    }
  }
  
  if (auto const Stmt = Links.getPrimaryStmt()) {
    if (HighlightedStmt != Stmt) {
      HighlightedStmt = Stmt;
      
      Notifier->createNotify<ConEvHighlightStmt>(HighlightedStmt);

      if (Recording) {
        Recording->recordEventL("ExplanationViewer.MouseOverStmtLink",
                                make_attribute("stmt", Stmt));
      }
    }
  }
  
  if (Links.getPrimaryIndex().indexOf("://") != -1) {
    SetCursor(wxCursor(wxCURSOR_HAND));
    URLHover = true;

    if (Recording) {
      std::string URL;
      Links.getPrimaryIndex().toUTF8String(URL);

      Recording->recordEventL("ExplanationViewer.MouseOverURL",
                              make_attribute("url", URL));
    }
  }
  else {
    URLClick = false;
  }
}

void ExplanationViewer::OnEnterWindow(wxMouseEvent &Event)
{
  if (Recording) {
    Recording->recordEventL("ExplanationViewer.MouseEnter");
  }

  Event.Skip();
}

void ExplanationViewer::OnLeaveWindow(wxMouseEvent &Event)
{
  if (Recording) {
    Recording->recordEventL("ExplanationViewer.MouseLeave");
  }

  clearCurrent();
  URLClick = false;
  Event.Skip();
}

void ExplanationViewer::OnLeftDown(wxMouseEvent &Event)
{
  if (URLHover) {
    URLClick = true;
  }
  else {
    URLClick = false;
    Event.Skip();
  }
}

void ExplanationViewer::OnLeftUp(wxMouseEvent &Event)
{
  if (URLClick) {
    // This is the "whole character" offset (regardless of the text's encoding).
    auto const Count = CountCharacters(0, CurrentMousePosition);
    
    auto const Links = Explanation->getCharacterLinksAt(Count);
    if (Links.getPrimaryIndex().isEmpty()) {
      return;
    }
    
    std::string URL;
    Links.getPrimaryIndex().toUTF8String(URL);

    if (Recording) {
      Recording->recordEventL("ExplanationViewer.MouseLeftClickURL",
                              make_attribute("url", URL));
    }

    ::wxLaunchDefaultBrowser(URL);
  }
  else {
    if (Recording) {
      Recording->recordEventL("ExplanationViewer.MouseLeftClick");
    }

    Event.Skip();
  }
}

void ExplanationViewer::show(std::shared_ptr<StateAccessToken> Access,
                             seec::cm::ProcessState const &Process,
                             seec::cm::ThreadState const &Thread)
{
  clearExplanation();
  
  if (!Access)
    return;
  
  auto Lock = Access->getAccess();
  if (!Lock)
    return;
  
  // Find the active function (if any).
  auto const &CallStack = Thread.getCallStack();
  if (CallStack.empty())
    return;
  
  auto const &Function = CallStack.back().get();
  
  // If there is an active Stmt then explain it. Otherwise, explain the active
  // function's Decl.
  auto const ActiveStmt = Function.getActiveStmt();
  if (ActiveStmt) {
    showExplanation(ActiveStmt, Function);
  }
  else {
    auto const FunctionDecl = Function.getFunctionDecl();
    if (FunctionDecl) {
      showExplanation(FunctionDecl);
    }
  }
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

void
ExplanationViewer::
showExplanation(::clang::Stmt const *Statement,
                ::seec::cm::FunctionState const &InFunction)
{
  auto MaybeExplanation =
    seec::clang_epv::explain(
      Statement,
      seec::clang_epv::makeRuntimeValueLookupByLambda(
        [&](::clang::Stmt const *S) -> bool {
          return InFunction.getStmtValue(S) ? true : false;
        },
        [&](::clang::Stmt const *S) -> std::string {
          auto const Value = InFunction.getStmtValue(S);
          return Value ? Value->getValueAsStringFull() : std::string();
        },
        [&](::clang::Stmt const *S) -> seec::Maybe<bool> {
          auto const Value = InFunction.getStmtValue(S);
          if (Value && Value->isCompletelyInitialized()
              && llvm::isa<seec::cm::ValueOfScalar>(*Value))
          {
            auto &Scalar = llvm::cast<seec::cm::ValueOfScalar>(*Value);
            return !Scalar.isZero();
          }
          return seec::Maybe<bool>();
        }));
  
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
    wxLogDebug("No explanation for Stmt of class %s.",
               Statement->getStmtClassName());
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
