//===- tools/seec-trace-view/SourceViewer.cpp -----------------------------===//
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

#include "seec/Clang/MappedStmt.hpp"
#include "seec/Clang/SourceMapping.hpp"
#include "seec/Clang/RuntimeValueMapping.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/RuntimeValue.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/Util/Range.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "clang/AST/Decl.h"
#include "clang/Lex/Lexer.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <wx/font.h>
#include <wx/tokenzr.h>
#include <wx/stc/stc.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "ExplanationViewer.hpp"
#include "HighlightEvent.hpp"
#include "SourceViewer.hpp"
#include "SourceViewerSettings.hpp"
#include "OpenTrace.hpp"

#include <list>

//------------------------------------------------------------------------------
// SourceFilePanel
//------------------------------------------------------------------------------

/// \brief Viewer for a single source code file.
///
class SourceFilePanel : public wxPanel {
  /// \brief Store information about an indicated region.
  ///
  struct IndicatedRegion {
    int Indicator;
    
    int Start;
    
    int Length;
    
    IndicatedRegion(int TheIndicator,
                    int RegionStart,
                    int RegionLength)
    : Indicator(TheIndicator),
      Start(RegionStart),
      Length(RegionLength)
    {}
  };
  
  
  /// Path to the file.
  llvm::sys::Path FilePath;

  /// Text control that displays the file.
  wxStyledTextCtrl *Text;
  
  /// Regions that have indicators for the current state.
  std::vector<IndicatedRegion> StateIndications;
  
  /// Lines that are annotated for the current state.
  std::vector<int> StateAnnotations;
  
  /// Regions that have temporary indicators (e.g. highlighting).
  std::list<IndicatedRegion> TemporaryIndicators;
  
  
  /// \brief Setup the Scintilla preferences.
  ///
  void setSTCPreferences() {
    // Set the lexer to C++.
    Text->SetLexer(wxSTC_LEX_CPP);
        
    // Setup the default common style settings.
    setupAllSciCommonTypes(*Text);
    
    // Setup the default style settings for the lexer.
    setupAllSciLexerTypes(*Text);
    
    // Setup the style settings for our indicators.
    setupAllSciIndicatorTypes(*Text);
    
    //
    UErrorCode Status = U_ZERO_ERROR;
    auto KeywordRes = seec::getResource("TraceViewer",
                                        Locale::getDefault(),
                                        Status,
                                        "ScintillaKeywords",
                                        "CPP");
    if (U_SUCCESS(Status)) {
      // Setup the keywords used by the lexer.
      auto Size = KeywordRes.getSize();
      
      for (int32_t i = 0; i < Size; ++i) {
        auto UniStr = KeywordRes.getStringEx(i, Status);
        if (U_FAILURE(Status))
          break;
        
        Text->SetKeyWords(i, seec::towxString(UniStr));
      }
    }

    // Setup the line number margin (initially invisible).
    Text->SetMarginType(static_cast<int>(SciMargin::LineNumber),
                        wxSTC_MARGIN_NUMBER);
    Text->SetMarginWidth(static_cast<int>(SciMargin::LineNumber), 0);
    
    // Annotations.
    Text->AnnotationSetVisible(wxSTC_ANNOTATION_STANDARD);
    
    // Miscellaneous.
    Text->SetIndentationGuides(true);
    Text->SetEdgeColumn(80);
    // Text->SetEdgeMode(wxSTC_EDGE_LINE);
    Text->SetWrapMode(wxSTC_WRAP_WORD);
    
    Text->SetExtraDescent(2);
  }
  
  /// \brief Handle file loading.
  ///
  void setFileSpecificOptions() {
    // Don't allow the user to edit the source code, because it will ruin our
    // mapping information.
    Text->SetReadOnly(true);
    
    // Set the width of the line numbers margin.
    auto LineCount = Text->GetLineCount();

    unsigned Digits = 1;
    while (LineCount /= 10)
      ++Digits;
    
    auto CharWidth = Text->TextWidth(wxSTC_STYLE_LINENUMBER, wxT("0"));
    
    auto MarginWidth = (Digits + 1) * CharWidth;
    
    Text->SetMarginWidth(static_cast<int>(SciMargin::LineNumber), MarginWidth);
    
    // Clear the selection.
    Text->Clear();
  }

public:
  /// Type used to reference temporary indicators.
  typedef decltype(TemporaryIndicators)::iterator
          temporary_indicator_token;
  
  // \brief Construct without creating.
  SourceFilePanel()
  : wxPanel(),
    FilePath(),
    Text(nullptr),
    StateIndications(),
    StateAnnotations(),
    TemporaryIndicators()
  {}

  // \brief Construct and create.
  SourceFilePanel(wxWindow *Parent,
                  llvm::sys::Path File,
                  wxWindowID ID = wxID_ANY,
                  wxPoint const &Position = wxDefaultPosition,
                  wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    FilePath(),
    Text(nullptr),
    StateIndications(),
    StateAnnotations(),
    TemporaryIndicators()
  {
    Create(Parent, File, ID, Position, Size);
  }

  /// \brief Destructor.
  virtual ~SourceFilePanel() {}

  /// \brief Create the panel.
  bool Create(wxWindow *Parent,
              llvm::sys::Path File,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize) {
    if (!wxPanel::Create(Parent, ID, Position, Size))
      return false;

    FilePath = File;

    Text = new wxStyledTextCtrl(this, wxID_ANY);

    // Setup the preferences of Text.
    setSTCPreferences();
    
    // Load the source code into the Scintilla control.
    if (!Text->LoadFile(FilePath.str())) {
      // TODO: Insert a localized error message.
    }
    
    setFileSpecificOptions();

    auto Sizer = new wxBoxSizer(wxHORIZONTAL);
    Sizer->Add(Text, wxSizerFlags().Proportion(1).Expand());
    SetSizerAndFit(Sizer);

    return true;
  }
  
  
  /// \name State display.
  /// @{
  
  /// \brief Clear state-related information.
  ///
  void clearState() {
    // Remove temporary indicators.
    for (auto &Region : StateIndications) {
      Text->SetIndicatorCurrent(Region.Indicator);
      Text->IndicatorClearRange(Region.Start, Region.Length);
    }
    
    StateIndications.clear();
  
    // Remove temporary annotations.
#if 0
    // There's no way to remove annotations from a single line with
    // wxStyledTextCtrl at the moment, so we just remove all annotations.
    for (auto Line : StateAnnotations) {
      Text->AnnotationSetText(Line, wxEmptyString);
    }
#else
    Text->AnnotationClearAll();
#endif
    
    StateAnnotations.clear();
    
    // wxStyledTextCtrl doesn't automatically redraw after the above.
    Text->Refresh();
  }
  
  /// \brief Set an indicator on a range of text for the current state.
  bool stateIndicatorAdd(SciIndicatorType Indicator, int Start, int End) {
    auto const IndicatorInt = static_cast<int>(Indicator);
    
    // Set the indicator on the text.
    Text->SetIndicatorCurrent(IndicatorInt);
    Text->IndicatorFillRange(Start, End - Start);
    
    // Save the indicator so that we can clear it in clearState().
    StateIndications.emplace_back(IndicatorInt, Start, End - Start);
    
    return true;
  }
  
  /// \brief Set an indicator on a range of text for this state.
  ///
  bool setStateIndicator(SciIndicatorType Indicator,
                         long StartLine,
                         long StartColumn,
                         long EndLine,
                         long EndColumn) {
    // Find the position for the new highlight.
    // wxTextCtrl line and column numbers are zero-based, whereas Clang's line
    // and column information is 1-based.
    int Start = Text->XYToPosition(StartColumn - 1, StartLine - 1);
    int End = Text->XYToPosition(EndColumn - 1, EndLine - 1);
    assert(Start != -1 && End != -1);
    
    return stateIndicatorAdd(Indicator, Start, End);
  }

  /// \brief Annotate a line for this state.
  ///
  void annotateLine(long Line,
                    wxString const &AnnotationText,
                    SciLexerType AnnotationStyle) {
    Text->AnnotationSetText(Line, AnnotationText);
    Text->AnnotationSetStyle(Line, static_cast<int>(AnnotationStyle));
    StateAnnotations.push_back(Line);
  }

  /// \brief Annotate a line for this state, starting at a set column.
  ///
  void annotateLine(long Line,
                    long Column,
                    wxString const &AnnotationText,
                    SciLexerType AnnotationStyle) {
    wxString Spacing(' ', Column);
    
    // Create spaced annotation text by placing Spacing prior to each line in
    // the AnnotationText.
    wxString SpacedText;
    wxStringTokenizer LineTokenizer(AnnotationText, "\n");
    while (LineTokenizer.HasMoreTokens())
      SpacedText << Spacing << LineTokenizer.GetNextToken();
    
    // Display the spaced annotation text.
    Text->AnnotationSetText(Line, SpacedText);
    Text->AnnotationSetStyle(Line, static_cast<int>(AnnotationStyle));
    StateAnnotations.push_back(Line);
  }
  
  /// @} (State display)
  
  
  /// \name Temporary display.
  /// @{
  
  /// \brief Add a new temporary indicator over the given range.
  ///
  temporary_indicator_token temporaryIndicatorAdd(SciIndicatorType Indicator,
                                                  int Start,
                                                  int End)
  {
    auto const IndicatorInt = static_cast<int>(Indicator);
    
    auto const Token = TemporaryIndicators.emplace(TemporaryIndicators.begin(),
                                                   IndicatorInt,
                                                   Start,
                                                   End - Start);
    
    Text->SetIndicatorCurrent(IndicatorInt);
    Text->IndicatorFillRange(Token->Start, Token->Length);
    
    return Token;
  }
  
  /// \brief Remove an existing temporary indicator.
  ///
  void temporaryIndicatorRemove(temporary_indicator_token Token)
  {
    Text->SetIndicatorCurrent(Token->Indicator);
    Text->IndicatorClearRange(Token->Start, Token->Length);
    
    TemporaryIndicators.erase(Token);
  }
  
  /// \brief Remove all existing temporary indicators.
  ///
  void temporaryIndicatorRemoveAll()
  {
    while (!TemporaryIndicators.empty()) {
      temporaryIndicatorRemove(TemporaryIndicators.begin());
    }
  }
  
  /// @} (Temporary display)
};


//------------------------------------------------------------------------------
// SourceViewerPanel
//------------------------------------------------------------------------------

SourceViewerPanel::~SourceViewerPanel() {}

bool SourceViewerPanel::Create(wxWindow *Parent,
                               OpenTrace const &TheTrace,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;

  Trace = &TheTrace;

  Notebook = new wxAuiNotebook(this,
                               wxID_ANY,
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxAUI_NB_TOP
                               | wxAUI_NB_TAB_SPLIT
                               | wxAUI_NB_TAB_MOVE
                               | wxAUI_NB_SCROLL_BUTTONS);
  
  ExplanationCtrl = new ExplanationViewer(this,
                                          wxID_ANY,
                                          wxDefaultPosition,
                                          wxSize(100, 100));

  auto TopSizer = new wxBoxSizer(wxVERTICAL);
  TopSizer->Add(Notebook, wxSizerFlags(1).Expand());
  TopSizer->Add(ExplanationCtrl, wxSizerFlags(0).Expand());
  SetSizerAndFit(TopSizer);
  
  // Setup highlight event handling.
  Bind(SEEC_EV_HIGHLIGHT_ON, &SourceViewerPanel::OnHighlightOn, this);
  
  Bind(SEEC_EV_HIGHLIGHT_OFF, &SourceViewerPanel::OnHighlightOff, this);

  // Load all source files.
  for (auto &MapGlobalPair : Trace->getMappedModule().getGlobalLookup()) {
    addSourceFile(MapGlobalPair.second.getFilePath());
  }

  return true;
}

void SourceViewerPanel::clear() {
  Notebook->DeleteAllPages();
  Pages.clear();
}

SourceFilePanel *SourceViewerPanel::showPageForFile(llvm::sys::Path const &File)
{
  // Find the SourceFilePanel.
  auto PageIt = Pages.find(File);
  if (PageIt == Pages.end()) {
    wxLogDebug("Couldn't find page for file.\n");
    return nullptr;
  }
  
  // Select the SourceFilePanel in the notebook.
  Notebook->SetSelection(Notebook->GetPageIndex(PageIt->second));
  
  return PageIt->second;
}

void SourceViewerPanel::show(seec::trace::ProcessState const &State) {
  // We want to show some information about the last action that modified the
  // shared state of the process. This can occur for one of three reasons:
  //  1) A function was entered (particularly main(), which causes argv and
  //     envp to be visible in the memory state).
  //  2) A function was exited.
  //  3) An instruction changed the state.
  //
  // For all of these, the first thing we need to do is find an event that
  // modified the shared process state during the most recent process time
  // update.
  
  // Clear existing state information from all files.
  for (auto &PagePair : Pages)
    PagePair.second->clearState();

  auto Time = State.getProcessTime();

  // We'll need to search each thread to find the event.
  for (auto &ThreadStatePtr : State.getThreadStates()) {
    // Get the most recent state modifier from this thread, and check if it was
    // within the most recent process time update.
    auto MaybeModifierRef = ThreadStatePtr->getLastProcessModifier();
    if (!MaybeModifierRef.assigned())
      continue;

    auto MaybeTime = MaybeModifierRef.get<0>()->getProcessTime();
    if (!MaybeTime.assigned())
      continue;

    if (MaybeTime.get<0>() != Time)
      continue;

    // This event is responsible for the most recent modification to the shared
    // process state. Now we have to find the event that it is subservient to.
    auto EvRef = MaybeModifierRef.get<0>();

    while (!EvRef->isBlockStart())
      --EvRef;

    switch (EvRef->getType()) {
      case seec::trace::EventType::FunctionStart:
        // Highlight the Function entry.
        {
          auto &StartEv = EvRef->as<seec::trace::EventType::FunctionStart>();
          auto &ThreadTrace = ThreadStatePtr->getTrace();
          auto FuncTrace = ThreadTrace.getFunctionTrace(StartEv.getRecord());
          auto Func = Trace->getModuleIndex().getFunction(FuncTrace.getIndex());
          assert(Func && "Couldn't find llvm::Function.");
          
          highlightFunctionEntry(Func);
        }
        break;

      case seec::trace::EventType::FunctionEnd:
        // Highlight the function exit.
        {
          auto &EndEv = EvRef->as<seec::trace::EventType::FunctionEnd>();
          auto &ThreadTrace = ThreadStatePtr->getTrace();
          auto FuncTrace = ThreadTrace.getFunctionTrace(EndEv.getRecord());
          auto Func = Trace->getModuleIndex().getFunction(FuncTrace.getIndex());
          assert(Func && "Couldn't find llvm::Function.");
          
          highlightFunctionExit(Func);
        }
        break;

      // Some kind of Instruction caused the update.
      default:
        {
          assert(EvRef->isInstruction()
                 && "Unexpected event owning shared state modifier.");

          // Find the function that contains the instruction.
          auto const &ThreadTrace = ThreadStatePtr->getTrace();
          auto MaybeFunctionTrace = ThreadTrace.getFunctionContaining(EvRef);
          assert(MaybeFunctionTrace.assigned());

          // Get the index of the llvm::Function and llvm::Instruction.
          auto FunctionIndex = MaybeFunctionTrace.get<0>().getIndex();
          auto InstructionIndex = EvRef->getIndex().get<0>();

          auto Lookup = Trace->getModuleIndex().getFunctionIndex(FunctionIndex);
          assert(Lookup && "Couldn't find FunctionIndex.");

          auto Instruction = Lookup->getInstruction(InstructionIndex);
          assert(Instruction && "Couldn't find Instruction.");
          
          // TODO: Re-enable.
          // highlightInstruction(Instruction, nullptr);
        }
        break;
    }

    // We found the last modifier, so stop searching.
    break;
  }
}

void SourceViewerPanel::show(seec::trace::ProcessState const &ProcessState,
                             seec::trace::ThreadState const &ThreadState) {
  // Clear existing state information from all files.
  for (auto &PagePair : Pages)
    PagePair.second->clearState();
  
  //
  auto RuntimeError = ThreadState.getCurrentError();
  if (RuntimeError) {
    wxLogDebug("RuntimeError");
  }

  // Find the active function.
  auto &CallStack = ThreadState.getCallStack();
  if (CallStack.empty())
    return;

  auto &FunctionState = *(CallStack.back());

  if (auto Instruction = FunctionState.getActiveInstruction()) {
    auto &RTValue = FunctionState.getRuntimeValue(Instruction);
    highlightInstruction(Instruction, RTValue, RuntimeError);
  }
  else {
    // If there is no active Instruction, highlight the function entry.
    auto FunctionIndex = FunctionState.getIndex();
    auto Func = Trace->getModuleIndex().getFunction(FunctionIndex);
    assert(Func && "Couldn't find llvm::Function.");
    
    highlightFunctionEntry(Func);
  }
}

SourceFileRange SourceViewerPanel::getRange(::clang::Decl const *Decl) const {
  auto const &AST = Decl->getASTContext();
  auto const &SourceManager = AST.getSourceManager();
  
  // Find the first character in the first token.
  auto Start = Decl->getLocStart();
  while (Start.isMacroID())
    Start = SourceManager.getExpansionLoc(Start);
  
  // Find the file that the first token belongs to.
  auto const FileID = SourceManager.getFileID(Start);
  auto const Filename = SourceManager.getFilename(Start);
  
  // Find the first character in the last token.
  auto End = Decl->getLocEnd();
  while (End.isMacroID())
    End = SourceManager.getExpansionLoc(End);
  
  // Find the first character following the last token.
  auto const FollowingEnd =
    clang::Lexer::getLocForEndOfToken(End,
                                      0,
                                      SourceManager,
                                      AST.getLangOpts());
  
  // Get the file offset of the start and end.
  auto const StartOffset = SourceManager.getFileOffset(Start);
  auto const EndOffset = FollowingEnd.isValid()
                       ? SourceManager.getFileOffset(End)
                       : SourceManager.getFileOffset(FollowingEnd);
  
  return SourceFileRange(Filename.str(),
                         StartOffset,
                         SourceManager.getLineNumber(FileID, StartOffset),
                         SourceManager.getColumnNumber(FileID, StartOffset),
                         EndOffset,
                         SourceManager.getLineNumber(FileID, EndOffset),
                         SourceManager.getColumnNumber(FileID, EndOffset));
}

SourceFileRange
SourceViewerPanel::getRange(::clang::Stmt const *Stmt,
                            ::clang::ASTContext const &AST) const
{
  auto const &SourceManager = AST.getSourceManager();
  
  // Find the first character in the first token.
  auto Start = Stmt->getLocStart();
  while (Start.isMacroID())
    Start = SourceManager.getExpansionLoc(Start);
  
  // Find the file that the first token belongs to.
  auto const FileID = SourceManager.getFileID(Start);
  auto const Filename = SourceManager.getFilename(Start);
  
  // Find the first character in the last token.
  auto End = Stmt->getLocEnd();
  while (End.isMacroID())
    End = SourceManager.getExpansionLoc(End);
  
  // Find the first character following the last token.
  auto const FollowingEnd =
    clang::Lexer::getLocForEndOfToken(End,
                                      0,
                                      SourceManager,
                                      AST.getLangOpts());
  
  // Get the file offset of the start and end.
  auto const StartOffset = SourceManager.getFileOffset(Start);
  
  auto const EndOffset = FollowingEnd.isValid()
                       ? SourceManager.getFileOffset(FollowingEnd)
                       : SourceManager.getFileOffset(End);
  
  return SourceFileRange(Filename.str(),
                         StartOffset,
                         SourceManager.getLineNumber(FileID, StartOffset),
                         SourceManager.getColumnNumber(FileID, StartOffset),
                         EndOffset,
                         SourceManager.getLineNumber(FileID, EndOffset),
                         SourceManager.getColumnNumber(FileID, EndOffset));
}

SourceFileRange SourceViewerPanel::getRange(::clang::Stmt const *Stmt) const {
  // This lookup is very inefficient.
  auto const MappedASTPtr = Trace->getMappedModule().getASTForStmt(Stmt);
  if (!MappedASTPtr) {
    wxLogDebug("highlightOn: couldn't find AST for Stmt.");
    return SourceFileRange();
  }
  
  return getRange(Stmt, MappedASTPtr->getASTUnit().getASTContext());
}

void SourceViewerPanel::highlightOn(::clang::Decl const *Decl) {
  auto const Range = getRange(Decl);
  if (Range.Filename.empty())
    return;
  
  // Find the SourceFilePanel for this file.
  llvm::sys::Path FilenamePath(Range.Filename);
  auto const PageIt = Pages.find(FilenamePath);
  if (PageIt == Pages.end()) {
    wxLogDebug("highlightOn: page not found for source file.");
    return;
  }
  
  // Now highlight this location.
  PageIt->second->temporaryIndicatorAdd(SciIndicatorType::CodeHighlight,
                                        Range.Start,
                                        Range.End);
}

void SourceViewerPanel::highlightOn(::clang::Stmt const *Stmt) {
  auto const Range = getRange(Stmt);
  if (Range.Filename.empty())
    return;
  
  // Find the SourceFilePanel for this file.
  llvm::sys::Path FilenamePath(Range.Filename);
  auto const PageIt = Pages.find(FilenamePath);
  if (PageIt == Pages.end()) {
    wxLogDebug("highlightOn: page not found for source file.");
    return;
  }
  
  // Now highlight this location.
  PageIt->second->temporaryIndicatorAdd(SciIndicatorType::CodeHighlight,
                                        Range.Start,
                                        Range.End);
}

void SourceViewerPanel::highlightOff() {
  for (auto &Page : Pages) {
    Page.second->temporaryIndicatorRemoveAll();
  }
}

void SourceViewerPanel::OnHighlightOn(HighlightEvent const &Ev) {
  switch (Ev.getType()) {
    case HighlightEvent::ItemType::Decl:
      highlightOn(Ev.getDecl());
      break;
    case HighlightEvent::ItemType::Stmt:
      highlightOn(Ev.getStmt());
      break;
  }
}

void SourceViewerPanel::OnHighlightOff(HighlightEvent const &Ev) {
  highlightOff();
}

void SourceViewerPanel::highlightFunctionEntry(llvm::Function *Function) {
  // Clear the current explanation.
  ExplanationCtrl->clearExplanation();
  
  // Get the Function mapping.
  auto Mapping = Trace->getMappedModule().getMappedGlobalDecl(Function);
  if (!Mapping) {
    wxLogDebug("No mapping for Function '%s'",
               Function->getName().str().c_str());
    return;
  }

  auto const Decl = Mapping->getDecl();  
  auto const Range = getRange(Decl);

  llvm::sys::Path FilePath(Range.Filename);

  auto Panel = showPageForFile(FilePath);
  if (!Panel)
    return;

  // TODO: Clear highlight on current source file?
  Panel->stateIndicatorAdd(SciIndicatorType::CodeActive,
                           Range.Start,
                           Range.End);
}

void SourceViewerPanel::highlightFunctionExit(llvm::Function *Function) {
  highlightFunctionEntry(Function);
}

void
SourceViewerPanel::showActiveRange(SourceFilePanel *Page,
                                   seec::seec_clang::SimpleRange const &Range)
{
  Page->setStateIndicator(SciIndicatorType::CodeActive,
                          Range.Start.Line, Range.Start.Column,
                          Range.End.Line, Range.End.Column);
}

void SourceViewerPanel::showActiveDecl(::clang::Decl const *Decl,
                                       seec::seec_clang::MappedAST const &AST)
{
  auto const Range = getRange(Decl);
  if (Range.Filename.empty()) {
    wxLogDebug("No range for Stmt.");
    return;
  }
  
  llvm::sys::Path FilePath(Range.Filename);
  auto Panel = showPageForFile(FilePath);
  if (!Panel) {
    wxLogDebug("No page for source file %s.", Range.Filename.c_str());
    return;
  }
  
  // Show that the Decl is active.
  Panel->stateIndicatorAdd(SciIndicatorType::CodeActive,
                           Range.Start,
                           Range.End);
    
  // Show an explanation for the Decl.
  ExplanationCtrl->showExplanation(Decl);
}

void SourceViewerPanel::showActiveStmt(::clang::Stmt const *Statement,
                                       seec::seec_clang::MappedAST const &AST,
                                       llvm::StringRef Value,
                                       wxString const &Error)
{
  auto &ClangAST = AST.getASTUnit();
  auto const Range = getRange(Statement, ClangAST.getASTContext());
  if (Range.Filename.empty()) {
    wxLogDebug("No range for Stmt.");
    return;
  }
  
  llvm::sys::Path FilePath(Range.Filename);
  auto Panel = showPageForFile(FilePath);
  if (!Panel) {
    wxLogDebug("No page for source file %s.", Range.Filename.c_str());
    return;
  }
  
  // Show that the Stmt is active.
  Panel->stateIndicatorAdd(SciIndicatorType::CodeActive,
                           Range.Start,
                           Range.End);
  
  // Show the value beneath the Stmt.
  if (!Value.empty()) {
    Panel->annotateLine(Range.EndLine - 1,
                        Range.StartColumn - 1,
                        wxString(Value),
                        SciLexerType::SeeCRuntimeValue);
  }
  
  // Show the error beneath the Stmt.
  if (!Error.empty()) {
    Panel->annotateLine(Range.EndLine - 1,
                        Error,
                        SciLexerType::SeeCRuntimeError);
  }
  
  // Show an explanation for the Stmt.
  ExplanationCtrl->showExplanation(Statement);
}

void SourceViewerPanel::highlightInstruction
      (llvm::Instruction const *Instruction,
       seec::trace::RuntimeValue const &Value,
       seec::runtime_errors::RunError const *Error) {
  assert(Trace);
  
  // Clear the current explanation.
  ExplanationCtrl->clearExplanation();
  
  // TODO: Clear state mapping on the current source file.

  auto &ClangMap = Trace->getMappedModule();
  
  // First, see if the Instruction represents a value that we can display.
  if (Value.assigned()) {
    auto Mappings = ClangMap.getMappedStmtsForValue(Instruction);
    
    for (auto &Mapping : seec::range(Mappings.first, Mappings.second)) {
      switch (Mapping.second->getMapType()) {
        case seec::seec_clang::MappedStmt::Type::LValSimple:
          // Not yet implemented.
          wxLogDebug("Unimplemented LValSimple.");
          break;
        
        case seec::seec_clang::MappedStmt::Type::RValScalar:
          {
            wxLogDebug("Showing RValScalar.");
            
            auto Statement = Mapping.second->getStatement();
            
            auto StrValue = seec::seec_clang::toString(Statement,
                                                       Instruction,
                                                       Value);
            
            // Highlight this clang::Stmt.
            showActiveStmt(Statement,
                           Mapping.second->getAST(),
                           StrValue,
                           wxEmptyString);
          }
          return;
        
        case seec::seec_clang::MappedStmt::Type::RValAggregate:
          // Not yet implemented.
          wxLogDebug("Unimplemented RValAggregate.");
          break;
      }
    }
  }
  
  // Find the clang::Stmt or clang::Decl that the Instruction belongs to.
  auto InstructionMap = ClangMap.getMapping(Instruction);
  if (!InstructionMap.getAST())
    return; // Instruction has no mapping.
  
  // Focus on the mapped source file.
  auto const Panel = showPageForFile(InstructionMap.getFilePath());
  if (!Panel)
    return;
  
  if (auto const Statement = InstructionMap.getStmt()) {
    wxString ErrorStr;
    
    if (Error)
      ErrorStr = seec::towxString(seec::runtime_errors::format(*Error));
    
    showActiveStmt(Statement, *InstructionMap.getAST(), "", ErrorStr);
  }
  else if (auto const Decl = InstructionMap.getDecl()) {
    showActiveDecl(Decl, *InstructionMap.getAST());
  }
}

void SourceViewerPanel::addSourceFile(llvm::sys::Path FilePath) {
  if (Pages.count(FilePath))
    return;

  auto SourcePanel = new SourceFilePanel(this, FilePath);

  Pages.insert(std::make_pair(FilePath, SourcePanel));

  Notebook->AddPage(SourcePanel, FilePath.c_str());
}

bool SourceViewerPanel::showSourceFile(llvm::sys::Path FilePath) {
  auto It = Pages.find(FilePath);
  if (It == Pages.end())
    return false;

  auto Index = Notebook->GetPageIndex(It->second);
  Notebook->SetSelection(Index);

  return true;
}
