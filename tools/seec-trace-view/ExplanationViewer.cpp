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
#include "seec/ICU/Indexing.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"

#include <wx/cursor.h>
#include <wx/utils.h>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "ColourSchemeSettings.hpp"
#include "ExplanationViewer.hpp"
#include "LocaleSettings.hpp"
#include "NotifyContext.hpp"
#include "OpenTrace.hpp"
#include "RuntimeValueLookup.hpp"
#include "SourceViewerSettings.hpp"
#include "StateAccessToken.hpp"
#include "TraceViewerApp.hpp"

#include <cassert>


std::pair<int, int>
ExplanationViewer::getAnnotationByteOffsetRange(int32_t const Start,
                                                int32_t const End)
{
  assert(Start <= End);

  // Initially set the offset to the first valid offset preceding the
  // "whole character" index. This will always be less than the required offset
  // (because no encoding uses less than one byte per character).
  int StartPos = PositionBefore(Start);

  // Find the "whole character" index of the initial position, use that to
  // determine how many characters away from the desired position we are, and
  // then iterate to the desired position.
  auto const StartGuessCount = CountCharacters(0, StartPos);
  for (int i = 0; i < Start - StartGuessCount; ++i)
    StartPos = PositionAfter(StartPos);

  // Get the EndPos by iterating from the StartPos.
  auto const Length = End - Start;
  int EndPos = StartPos;
  for (int i = 0; i < Length; ++i)
    EndPos = PositionAfter(EndPos);

  return std::make_pair(StartPos, EndPos);
}

std::pair<int, int>
ExplanationViewer::getExplanationByteOffsetRange(int32_t const Start,
                                                 int32_t const End)
{
  auto const AnnotationText = Annotation->getText();
  return getAnnotationByteOffsetRange(Start + AnnotationText.length(),
                                      End   + AnnotationText.length());
}

void ExplanationViewer::setAnnotationText(wxString const &Value)
{
  auto MaybeIndexed = IndexedAnnotationText::create(Trace->getTrace(), Value);
  if (!MaybeIndexed.assigned<IndexedAnnotationText>())
    return;

  Annotation = seec::makeUnique<IndexedAnnotationText>
                               (MaybeIndexed.move<IndexedAnnotationText>());

  this->SetEditable(true);
  auto const ExplanationLength = GetLength() - AnnotationLength;
  this->Replace(0, AnnotationLength, Annotation->getText());
  AnnotationLength = GetLength() - ExplanationLength;
  this->SetEditable(false);
  this->ClearSelections();

  // Set indicators for the indexed parts of the annotation.
  SetIndicatorCurrent(static_cast<int>(SciIndicatorType::TextInteractive));

  for (auto const &Pair : Annotation->getIndexedString().getNeedleLookup()) {
    auto const &Needle = Pair.second;
    auto const Range = getAnnotationByteOffsetRange(Needle.getStart(),
                                                    Needle.getEnd());
    IndicatorFillRange(Range.first, Range.second - Range.first);
  }
}

void ExplanationViewer::setExplanationText(wxString const &Value)
{
  this->SetEditable(true);
  this->Replace(AnnotationLength, this->GetLength(), Value);
  this->SetEditable(false);
  this->ClearSelections();
}

void ExplanationViewer::setExplanationIndicators()
{
  if (!Explanation)
    return;

  SetIndicatorCurrent(static_cast<int>(SciIndicatorType::TextInteractive));

  auto const &Indexed = Explanation->getIndexedString();

  for (auto const &NeedlePair : Indexed.getNeedleLookup()) {
    auto const &Needle = NeedlePair.second;
    auto const Range = getExplanationByteOffsetRange(Needle.getStart(),
                                                     Needle.getEnd());
    IndicatorFillRange(Range.first, Range.second - Range.first);
  }
}

void ExplanationViewer::mouseOverDecl(clang::Decl const *TheDecl)
{
  if (HighlightedDecl != TheDecl) {
    HighlightedDecl = TheDecl;

    Notifier->createNotify<ConEvHighlightDecl>(HighlightedDecl);

    if (Recording) {
      Recording->recordEventL("ExplanationViewer.MouseOverDeclLink",
                              make_attribute("decl", TheDecl));
    }
  }
}

void ExplanationViewer::mouseOverStmt(clang::Stmt const *TheStmt)
{
  if (HighlightedStmt != TheStmt) {
    HighlightedStmt = TheStmt;

    Notifier->createNotify<ConEvHighlightStmt>(HighlightedStmt);

    if (Recording) {
      Recording->recordEventL("ExplanationViewer.MouseOverStmtLink",
                              make_attribute("stmt", TheStmt));
    }
  }
}

void ExplanationViewer::mouseOverHyperlink(UnicodeString const &URL)
{
  SetCursor(wxCursor(wxCURSOR_HAND));
  URLHover = true;
  URLHovered.clear();
  URL.toUTF8String(URLHovered);

  if (Recording) {
    Recording->recordEventL("ExplanationViewer.MouseOverURL",
                            make_attribute("url", URLHovered));
  }
}

void ExplanationViewer::clearCurrent()
{
  SetIndicatorCurrent(static_cast<int>(SciIndicatorType::CodeHighlight));
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

void ExplanationViewer::updateColourScheme(ColourScheme const &Scheme)
{
  setupStylesFromColourScheme(*this, Scheme);
}

ExplanationViewer::~ExplanationViewer() {}

bool ExplanationViewer::Create(wxWindow *Parent,
                               OpenTrace &WithTrace,
                               ContextNotifier &WithNotifier,
                               ActionRecord &WithRecording,
                               ActionReplayFrame &WithReplay,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size)
{
  if (!wxStyledTextCtrl::Create(Parent, ID, Position, Size))
    return false;
  
  Trace = &WithTrace;
  Notifier = &WithNotifier;
  Recording = &WithRecording;
  
  Bind(wxEVT_MOTION, &ExplanationViewer::OnMotion, this);
  Bind(wxEVT_ENTER_WINDOW, &ExplanationViewer::OnEnterWindow, this);
  Bind(wxEVT_LEAVE_WINDOW, &ExplanationViewer::OnLeaveWindow, this);
  Bind(wxEVT_LEFT_DOWN, &ExplanationViewer::OnLeftDown, this);
  Bind(wxEVT_LEFT_UP, &ExplanationViewer::OnLeftUp, this);

  updateColourScheme(*(wxGetApp().getColourSchemeSettings().getColourScheme()));

  // Handle ColourSchemeSettings changes.
  wxGetApp().getColourSchemeSettings().addListener(
    [this] (ColourSchemeSettings const &Settings) {
      updateColourScheme(*Settings.getColourScheme());
    });
  
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
    URLHovered.clear();
    return;
  }
  
  if (CurrentMousePosition < AnnotationLength) {
    // This is the "whole character" offset into the annotation.
    auto const Count = CountCharacters(0, Pos);

    auto const MaybeIndex = Annotation->getPrimaryIndexAt(Count);
    if (!MaybeIndex.assigned<AnnotationIndex>())
      return;

    auto const &Index = MaybeIndex.get<AnnotationIndex>();

    SetIndicatorCurrent(static_cast<int>(SciIndicatorType::CodeHighlight));

    auto const Range = getAnnotationByteOffsetRange(Index.getStart(),
                                                    Index.getEnd());

    IndicatorFillRange(Range.first, Range.second - Range.first);

    if (auto const TheDecl = Index.getDecl())
      mouseOverDecl(TheDecl);

    if (auto const TheStmt = Index.getStmt())
      mouseOverStmt(TheStmt);

    if (Index.getIndex().indexOf("://") != -1)
      mouseOverHyperlink(Index.getIndex());
    else {
      URLClick = false;
      URLHovered.clear();
    }
  }
  else {
    // This is the "whole character" offset (regardless of the text's encoding).
    auto const Count = CountCharacters(AnnotationLength, Pos);

    auto const Links = Explanation->getCharacterLinksAt(Count);
    if (Links.getPrimaryIndex().isEmpty())
      return;

    SetIndicatorCurrent(static_cast<int>(SciIndicatorType::CodeHighlight));

    auto const Range =
      getExplanationByteOffsetRange(Links.getPrimaryIndexStart(),
                                    Links.getPrimaryIndexEnd());

    IndicatorFillRange(Range.first, Range.second - Range.first);

    if (auto const Decl = Links.getPrimaryDecl())
      mouseOverDecl(Decl);

    if (auto const Stmt = Links.getPrimaryStmt())
      mouseOverStmt(Stmt);

    if (Links.getPrimaryIndex().indexOf("://") != -1)
      mouseOverHyperlink(Links.getPrimaryIndex());
    else {
      URLClick = false;
      URLHovered.clear();
    }
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
    URLHovered.clear();
    Event.Skip();
  }
}

void ExplanationViewer::OnLeftUp(wxMouseEvent &Event)
{
  if (URLClick) {
    if (Recording) {
      Recording->recordEventL("ExplanationViewer.MouseLeftClickURL",
                              make_attribute("url", URLHovered));
    }

    ::wxLaunchDefaultBrowser(URLHovered);
  }
  else {
    if (Recording) {
      Recording->recordEventL("ExplanationViewer.MouseLeftClick");
    }

    Event.Skip();
  }
}

bool ExplanationViewer::showAnnotations(seec::cm::ProcessState const &Process,
                                        seec::cm::ThreadState const &Thread)
{
  // Get annotation for this process state (if any).
  auto &Annotations = Trace->getAnnotations();
  wxString CombinedText;

  auto const MaybeProcessAnno = Annotations.getPointForProcessState(Process);
  if (MaybeProcessAnno.assigned<AnnotationPoint>()) {
    auto const Text = MaybeProcessAnno.get<AnnotationPoint>().getText();
    if (!Text.empty()) {
      CombinedText << Text << "\n";
    }
  }

  auto const MaybeThreadAnno = Annotations.getPointForThreadState(Thread);
  if (MaybeThreadAnno.assigned<AnnotationPoint>()) {
    auto const Text = MaybeThreadAnno.get<AnnotationPoint>().getText();
    if (!Text.empty()) {
      if (!CombinedText.empty())
        CombinedText << "\n";
      CombinedText << Text << "\n";
    }
  }

  bool SuppressEPV = false;
  auto const &CallStack = Thread.getCallStack();
  if (!CallStack.empty()) {
    auto const &Function = CallStack.back().get();
    seec::Maybe<AnnotationPoint> MaybeAnno;

    if (auto const ActiveStmt = Function.getActiveStmt())
      MaybeAnno = Annotations.getPointForNode(Trace->getTrace(), ActiveStmt);
    else if (auto const FunctionDecl = Function.getFunctionDecl())
      MaybeAnno = Annotations.getPointForNode(Trace->getTrace(), FunctionDecl);

    if (MaybeAnno.assigned<AnnotationPoint>()) {
      auto const &Point = MaybeAnno.get<AnnotationPoint>();

      auto const Text = Point.getText();
      if (!Text.empty()) {
        if (!CombinedText.empty())
          CombinedText << "\n";
        CombinedText << Text << "\n";
      }

      if (Point.hasSuppressEPV())
        SuppressEPV = true;
    }
  }

  if (!CombinedText.empty()) {
    CombinedText << "\n";
    setAnnotationText(CombinedText);
  }

  return SuppressEPV;
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
  
  auto const SuppressEPV = showAnnotations(Process, Thread);

  if (!SuppressEPV) {
    // Find the active function (if any).
    auto const &CallStack = Thread.getCallStack();
    if (!CallStack.empty()) {
      auto const &Function = CallStack.back().get();

      // If there is an active Stmt then explain it. Otherwise, explain the
      // active function's Decl.
      if (auto const ActiveStmt = Function.getActiveStmt()) {
        showExplanation(ActiveStmt, Function);
      }
      else if (auto const FunctionDecl = Function.getFunctionDecl()) {
        showExplanation(FunctionDecl);
      }
    }
  }
}

void ExplanationViewer::showExplanation(::clang::Decl const *Decl)
{
  auto const &Augmentations = wxGetApp().getAugmentations();
  auto MaybeExplanation =
    seec::clang_epv::explain(Decl, Augmentations.getCallbackFn());
  
  if (MaybeExplanation.assigned(0)) {
    Explanation = std::move(MaybeExplanation.get<0>());
    setExplanationText(seec::towxString(Explanation->getString()));
    setExplanationIndicators();
  }
  else if (MaybeExplanation.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto String = MaybeExplanation.get<seec::Error>().getMessage(Status,
                                                                 getLocale());
    if (U_SUCCESS(Status)) {
      setExplanationText(seec::towxString(String));
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
  auto const &Augmentations = wxGetApp().getAugmentations();
  auto MaybeExplanation =
    seec::clang_epv::explain(
      Statement,
      RuntimeValueLookupForFunction(&InFunction),
      Augmentations.getCallbackFn());
  
  if (MaybeExplanation.assigned(0)) {
    Explanation = std::move(MaybeExplanation.get<0>());
    setExplanationText(seec::towxString(Explanation->getString()));
    setExplanationIndicators();
  }
  else if (MaybeExplanation.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto String = MaybeExplanation.get<seec::Error>().getMessage(Status,
                                                                 getLocale());
    if (U_SUCCESS(Status)) {
      setExplanationText(seec::towxString(String));
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
  
  // Discard the annotation.
  setAnnotationText(wxEmptyString);

  // Discard the explanation and clear the display.
  Explanation.reset();
  setExplanationText(wxEmptyString);
}
