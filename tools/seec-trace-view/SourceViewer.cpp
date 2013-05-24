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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Util/Range.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <wx/font.h>
#include <wx/tokenzr.h>
#include <wx/stc/stc.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "ExplanationViewer.hpp"
#include "HighlightEvent.hpp"
#include "OpenTrace.hpp"
#include "SourceViewer.hpp"
#include "SourceViewerSettings.hpp"
#include "TraceViewerFrame.hpp"

#include <list>


//------------------------------------------------------------------------------
// SourceFileRange
//------------------------------------------------------------------------------

/// \brief A range in a source file.
///
struct SourceFileRange {
  clang::FileEntry const *File;
  
  unsigned Start;
  
  unsigned StartLine;
  
  unsigned StartColumn;
  
  unsigned End;
  
  unsigned EndLine;
  
  unsigned EndColumn;
  
  SourceFileRange()
  : File(nullptr),
    Start(0),
    StartLine(0),
    StartColumn(0),
    End(0),
    EndLine(0),
    EndColumn(0)
  {}
  
  SourceFileRange(clang::FileEntry const *WithFile,
                  unsigned WithStart,
                  unsigned WithStartLine,
                  unsigned WithStartColumn,
                  unsigned WithEnd,
                  unsigned WithEndLine,
                  unsigned WithEndColumn)
  : File(WithFile),
    Start(WithStart),
    StartLine(WithStartLine),
    StartColumn(WithStartColumn),
    End(WithEnd),
    EndLine(WithEndLine),
    EndColumn(WithEndColumn)
  {}
};

/// \brief Get the range in the outermost file of two locations.
///
static SourceFileRange getRangeOutermost(clang::SourceLocation Start,
                                         clang::SourceLocation End,
                                         clang::ASTContext const &AST)
{
  auto const &SourceManager = AST.getSourceManager();
  
  // Find the first character in the first token.
  while (Start.isMacroID())
    Start = SourceManager.getExpansionLoc(Start);
  
  // Find the file that the first token belongs to.
  auto const FileID = SourceManager.getFileID(Start);
  auto const File = SourceManager.getFileEntryForID(FileID);
  
  // Find the first character in the last token.
  while (End.isMacroID())
    End = SourceManager.getExpansionLoc(End);
  
  if (SourceManager.getFileID(End) != FileID)
    return SourceFileRange{};
  
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
  
  return SourceFileRange(File,
                         StartOffset,
                         SourceManager.getLineNumber(FileID, StartOffset),
                         SourceManager.getColumnNumber(FileID, StartOffset),
                         EndOffset,
                         SourceManager.getLineNumber(FileID, EndOffset),
                         SourceManager.getColumnNumber(FileID, EndOffset));
}

/// \brief Get the range in the outermost file that contains a Stmt.
///
static SourceFileRange getRangeOutermost(::clang::Stmt const *Stmt,
                                         ::clang::ASTContext const &AST)
{
  return getRangeOutermost(Stmt->getLocStart(), Stmt->getLocEnd(), AST);
}

/// \brief Get the range in the outermost file that contains a Decl.
///
static SourceFileRange getRangeOutermost(::clang::Decl const *Decl,
                                         ::clang::ASTContext const &AST)
{
  return getRangeOutermost(Decl->getLocStart(), Decl->getLocEnd(), AST);
}


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
    Text(nullptr),
    StateIndications(),
    StateAnnotations(),
    TemporaryIndicators()
  {}

  // \brief Construct and create.
  SourceFilePanel(wxWindow *Parent,
                  llvm::MemoryBuffer const &Buffer,
                  wxWindowID ID = wxID_ANY,
                  wxPoint const &Position = wxDefaultPosition,
                  wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    Text(nullptr),
    StateIndications(),
    StateAnnotations(),
    TemporaryIndicators()
  {
    Create(Parent, Buffer, ID, Position, Size);
  }

  /// \brief Destructor.
  virtual ~SourceFilePanel() {}

  /// \brief Create the panel.
  bool Create(wxWindow *Parent,
              llvm::MemoryBuffer const &Buffer,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize) {
    if (!wxPanel::Create(Parent, ID, Position, Size))
      return false;

    Text = new wxStyledTextCtrl(this, wxID_ANY);

    // Setup the preferences of Text.
    setSTCPreferences();
    
    // Load the source code into the Scintilla control.
    Text->SetText(wxString(Buffer.getBufferStart(), Buffer.getBufferSize()));
    
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

SourceViewerPanel::SourceViewerPanel()
: wxPanel(),
  Notebook(nullptr),
  Trace(nullptr),
  Pages(),
  CurrentAccess()
{}

SourceViewerPanel::SourceViewerPanel(wxWindow *Parent,
                                     OpenTrace const &TheTrace,
                                     wxWindowID ID,
                                     wxPoint const &Position,
                                     wxSize const &Size)
: wxPanel(),
  Notebook(nullptr),
  Trace(nullptr),
  Pages(),
  CurrentAccess()
{
  Create(Parent, TheTrace, ID, Position, Size);
}

SourceViewerPanel::~SourceViewerPanel()
{}

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
  Bind(SEEC_EV_HIGHLIGHT_ON,  &SourceViewerPanel::OnHighlightOn,  this);  
  Bind(SEEC_EV_HIGHLIGHT_OFF, &SourceViewerPanel::OnHighlightOff, this);

  // TODO: Load all source files.
  
  return true;
}

void SourceViewerPanel::clear() {
  Notebook->DeleteAllPages();
  Pages.clear();
}

void SourceViewerPanel::show(std::shared_ptr<StateAccessToken> Access,
                             seec::cm::ProcessState const &Process,
                             seec::cm::ThreadState const &Thread)
{
  // Clear any existing explanation.
  ExplanationCtrl->clearExplanation();
  
  // Clear existing state information from all files.
  for (auto &PagePair : Pages)
    PagePair.second->clearState();
  
  // Replace our old access token.
  CurrentAccess = std::move(Access);
  if (!CurrentAccess)
    return;
  
  // Lock the current state while we read from it.
  auto Lock = CurrentAccess->getAccess();
  if (!Lock)
    return;
  
  // Find the active function (if any).
  auto const &CallStack = Thread.getCallStack();
  if (CallStack.empty())
    return;
  
  auto const &Function = CallStack.back().get();
  
  // TODO: Show active runtime errors.
  
  auto const ActiveStmt = Function.getActiveStmt();
  if (ActiveStmt) {
    showActiveStmt(ActiveStmt, Function);
  }
  
  // TODO: We could highlight the Function if !ActiveStmt.
}

void
SourceViewerPanel::showActiveStmt(::clang::Stmt const *Statement,
                                  ::seec::cm::FunctionState const &InFunction)
{
  auto const MappedAST = InFunction.getMappedAST();
  if (!MappedAST)
    return;
  
  auto &ASTUnit = MappedAST->getASTUnit();
  
  auto const Range = getRangeOutermost(Statement, ASTUnit.getASTContext());
  
  if (!Range.File) {
    wxLogDebug("Couldn't find file for Stmt.");
    return;
  }
  
  auto const Panel = loadAndShowFile(Range.File, *MappedAST);
  if (!Panel) {
    wxLogDebug("Couldn't show source panel for file %s.",
               Range.File->getName());
    return;
  }
  
  // Show that the Stmt is active.
  Panel->stateIndicatorAdd(SciIndicatorType::CodeActive,
                           Range.Start,
                           Range.End);
  
  auto const Value = InFunction.getStmtValue(Statement);
  if (Value) {
    Panel->annotateLine(Range.EndLine - 1,
                        Range.StartColumn - 1,
                        wxString(Value->getValueAsStringFull()),
                        SciLexerType::SeeCRuntimeValue);
  }
  
#if 0 // TODO.
    wxString ErrorStr;
    if (Error) {
      using namespace seec::runtime_errors;
      
      auto MaybeDesc = Description::create(*Error);
      
      wxASSERT(MaybeDesc.assigned());
      
      if (MaybeDesc.assigned(0)) {
        DescriptionPrinterUnicode Printer(std::move(MaybeDesc.get<0>()),
                                          "\n",
                                          "  ");
        
        ErrorStr = seec::towxString(Printer.getString());
      }
      else if (MaybeDesc.assigned<seec::Error>()) {
        UErrorCode Status = U_ZERO_ERROR;
        ErrorStr = seec::towxString(MaybeDesc.get<seec::Error>()
                                             .getMessage(Status, Locale()));
      }
    }
    
  // Show the error beneath the Stmt.
  if (!Error.empty()) {
    Panel->annotateLine(Range.EndLine - 1,
                        Error,
                        SciLexerType::SeeCRuntimeError);
  }
#endif
  
  // Show an explanation for the Stmt.
  ExplanationCtrl->showExplanation(Statement, InFunction);
}

SourceFilePanel *
SourceViewerPanel::loadAndShowFile(clang::FileEntry const *File,
                                   seec::seec_clang::MappedAST const &MAST)
{
  auto const It = Pages.find(File);
  if (It != Pages.end()) {
    auto const Index = Notebook->GetPageIndex(It->second);
    Notebook->SetSelection(Index);
    return It->second;
  }
  
  // Create a new panel for this file.
  auto &ASTUnit = MAST.getASTUnit();
  auto &SrcMgr = ASTUnit.getSourceManager();
  
  bool Invalid = false;
  auto const Buffer = SrcMgr.getMemoryBufferForFile(File, &Invalid);
  
  if (Invalid)
    return nullptr;
  
  auto const SourcePanel = new SourceFilePanel(this, *Buffer);
  Pages.insert(std::make_pair(File, SourcePanel));
  Notebook->AddPage(SourcePanel, File->getName());
  
  return SourcePanel;
}

void SourceViewerPanel::highlightOn(::clang::Decl const *Decl) {
  if (!Trace)
    return;
  
  auto const &ProcessTrace = Trace->getTrace();
  auto const &MappedModule = ProcessTrace.getMapping();
  auto const MappedAST = MappedModule.getASTForDecl(Decl);
  if (!MappedAST)
    return;
  
  auto const &ASTUnit = MappedAST->getASTUnit();
  auto const Range = getRangeOutermost(Decl, ASTUnit.getASTContext());
  
  if (!Range.File)
    return;
  
  auto const PageIt = Pages.find(Range.File);
  if (PageIt == Pages.end())
    return;
  
  PageIt->second->temporaryIndicatorAdd(SciIndicatorType::CodeHighlight,
                                        Range.Start,
                                        Range.End);
}

void SourceViewerPanel::highlightOn(::clang::Stmt const *Stmt) {
  if (!Trace)
    return;
  
  auto const &ProcessTrace = Trace->getTrace();
  auto const &MappedModule = ProcessTrace.getMapping();
  auto const MappedAST = MappedModule.getASTForStmt(Stmt);
  if (!MappedAST)
    return;
  
  auto const &ASTUnit = MappedAST->getASTUnit();
  auto const Range = getRangeOutermost(Stmt, ASTUnit.getASTContext());
  
  if (!Range.File)
    return;
  
  auto const PageIt = Pages.find(Range.File);
  if (PageIt == Pages.end())
    return;
  
  PageIt->second->temporaryIndicatorAdd(SciIndicatorType::CodeHighlight,
                                        Range.Start,
                                        Range.End);
}

void SourceViewerPanel::highlightOff() {
  for (auto &Page : Pages)
    Page.second->temporaryIndicatorRemoveAll();
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
