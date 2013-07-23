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
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/LineWrapper.hpp"
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

#include "unicode/brkiter.h"

#include "ExplanationViewer.hpp"
#include "HighlightEvent.hpp"
#include "OpenTrace.hpp"
#include "SourceViewer.hpp"
#include "SourceViewerSettings.hpp"
#include "TraceViewerFrame.hpp"

#include <list>
#include <map>
#include <memory>


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
// Annotation
//------------------------------------------------------------------------------

/// \brief Control wrapping for annotations.
///
enum class WrapStyle {
  None,
  Wrapped
};

/// \brief Text and settings for an annotation.
///
class Annotation {
  /// The text of this annotation.
  UnicodeString Text;
  
  /// The style to use for this annotation's text.
  SciLexerType Style;
  
  /// The wrapping style for this annotation.
  WrapStyle Wrapping;
  
  /// Indent each line with this many spaces.
  long Indent;
  
public:
  /// \brief Constructor.
  ///
  Annotation(UnicodeString const &WithText,
             SciLexerType const WithStyle,
             WrapStyle const WithWrapping)
  : Text(WithText),
    Style(WithStyle),
    Wrapping(WithWrapping),
    Indent(0)
  {}
  
  /// \brief Accessors.
  /// @{
  
  UnicodeString const &getText() const {
    return Text;
  }
  
  SciLexerType getStyle() const {
    return Style;
  }
  
  WrapStyle getWrapping() const {
    return Wrapping;
  }
  
  long getIndent() const {
    return Indent;
  }
  
  /// @} (Accessors)
  
  /// \brief Mutators.
  /// @{
  
  /// \brief Set the indentation of this annotation.
  ///
  void setIndent(long const Value) {
    Indent = Value;
  }
  
  /// @} (Mutators)
};


//------------------------------------------------------------------------------
// SourceFilePanel
//------------------------------------------------------------------------------

wxDEFINE_EVENT(EVT_SOURCE_ANNOTATION_RERENDER, wxCommandEvent);

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
  
  /// Used to perform line wrapping.
  std::unique_ptr<BreakIterator> Breaker;
  
  /// Regions that have indicators for the current state.
  std::vector<IndicatedRegion> StateIndications;
  
  /// Annotations for the current state, indexed by line.
  std::multimap<int, Annotation> StateAnnotations;
  
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
    Text->SetWrapMode(wxSTC_WRAP_NONE);
    
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
  
  /// \brief Render annotations for a specific line.
  ///
  /// Wrapped annotations do not currently support indentation. If an annotation
  /// has non-zero indentation and WrapStyle::Wrapped, then the indentation will
  /// be ignored.
  ///
  void renderAnnotationsFor(int const Line) {
    // Calculate the width of the text region, in case we do any wrapping.
    auto const MarginLineNumber = static_cast<int>(SciMargin::LineNumber);
    auto const ClientSize = Text->GetClientSize();
    auto const Width = ClientSize.GetWidth()
                       - Text->GetMarginWidth(MarginLineNumber);
    
    wxString CompleteString;
    wxString Styles;
    
    for (auto const &LA : seec::range(StateAnnotations.equal_range(Line))) {
      auto const &Anno = LA.second;
      auto const &AnnoText = Anno.getText();
      auto const Indent = Anno.getIndent();
      auto const Style = static_cast<int>(Anno.getStyle());
      wxString Spacing(' ', Indent);
      
      switch (Anno.getWrapping()) {
        case WrapStyle::None:
        {
          if (!CompleteString.IsEmpty())
            CompleteString += "\n";
          
          auto const Length = AnnoText.length();
          int32_t FragStart = 0;
          
          while (FragStart < Length) {
            auto const NewlineIdx = AnnoText.indexOf('\n', FragStart);
            auto const FragEnd = NewlineIdx != -1 ? NewlineIdx : Length;
            
            CompleteString += Spacing;
            CompleteString +=
              seec::towxString(AnnoText.tempSubStringBetween(FragStart,
                                                             FragEnd));
            FragStart = FragEnd + 1;
          }
          
          break;
        }
        
        case WrapStyle::Wrapped:
        {
          auto const Wrappings =
            seec::wrapParagraph(*Breaker, AnnoText,
              [=] (UnicodeString const &Line) -> bool {
                return Text->TextWidth(Style, seec::towxString(Line)) < Width;
              });
          
          for (auto const &Wrapping : Wrappings) {
            if (!CompleteString.IsEmpty())
              CompleteString += "\n";
            
            auto const Limit = Wrapping.End - Wrapping.TrailingWhitespace;
            CompleteString +=
              seec::towxString(AnnoText.tempSubStringBetween(Wrapping.Start,
                                                             Limit));
          }
          
          break;
        }
      }
      
      auto const NumCharsAdded = CompleteString.size() - Styles.size();
      if (NumCharsAdded)
        Styles.Append(static_cast<char>(Style), NumCharsAdded);
    }
    
    Text->AnnotationSetText(Line, CompleteString);
    Text->AnnotationSetStyles(Line, Styles);
  }
  
  /// \brief Render all annotations.
  ///
  void renderAnnotations() {
    for (auto It = StateAnnotations.begin(), End = StateAnnotations.end();
         It != End;
         It = StateAnnotations.upper_bound(It->first))
    {
      renderAnnotationsFor(It->first);
    }
  }

public:
  /// Type used to reference temporary indicators.
  typedef decltype(TemporaryIndicators)::iterator
          temporary_indicator_token;
  
  // \brief Construct without creating.
  SourceFilePanel()
  : wxPanel(),
    Text(nullptr),
    Breaker(nullptr),
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
    Breaker(nullptr),
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
    
    // Setup the BreakIterator used for line wrapping.
    UErrorCode Status = U_ZERO_ERROR;
    Breaker.reset(BreakIterator::createLineInstance(Locale(), Status));
    
    if (U_FAILURE(Status)) {
      Breaker.reset();
      return false;
    }
    
    // When the window is resized, create an event to rerender the annotations.
    // We can't rerender them immediately, because the size of the Text control
    // won't have been updated yet.
    Bind(wxEVT_SIZE, std::function<void (wxSizeEvent &)> {
      [this] (wxSizeEvent &Ev) {
        Ev.Skip();
        
        wxCommandEvent EvRerender {
          EVT_SOURCE_ANNOTATION_RERENDER,
          this->GetId()
        };
        
        EvRerender.SetEventObject(this);
        this->AddPendingEvent(EvRerender);
      }});
    
    // Handle the event to rerender annotations (created by resizing).
    Bind(EVT_SOURCE_ANNOTATION_RERENDER,
      std::function<void (wxCommandEvent &)> {
        [this] (wxCommandEvent &Ev) {
          renderAnnotations();
        }});

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
    for (auto const &LineAnno : StateAnnotations) {
      Text->AnnotationClearLine(LineAnno.first);
    }
    
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
  /// \param Line The line to add the annotation on.
  /// \param Column Indent the annotation to start on this column.
  /// \param AnnotationText The text of the annotation.
  /// \param AnnotationStyle The style to use for the annotation.
  /// \param Wrapping Whether or not the annotation should be wrapped to fit
  ///                 the width of the source panel.
  ///
  void annotateLine(long const Line,
                    long const Column,
                    UnicodeString const &AnnotationText,
                    SciLexerType const AnnotationStyle,
                    WrapStyle const Wrapping)
  {
    auto const CharPosition = Text->PositionFromLine(Line) + Column;
    auto const RealColumn = Text->GetColumn(CharPosition);
    
    auto Anno = Annotation{AnnotationText, AnnotationStyle, Wrapping};
    Anno.setIndent(RealColumn);
    
    auto const IntLine = static_cast<int>(Line);
    
    StateAnnotations.insert(std::make_pair(IntLine, std::move(Anno)));
    
    renderAnnotationsFor(IntLine);
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
  
  auto const ActiveStmt = Function.getActiveStmt();
  if (ActiveStmt) {
    showActiveStmt(ActiveStmt, Function);
  }
  else {
    auto const FunctionDecl = Function.getFunctionDecl();
    if (FunctionDecl) {
      showActiveDecl(FunctionDecl, Function);
    }
  }
  
  // Show all active runtime errors.
  for (auto const &RuntimeError : Function.getRuntimeErrorsActive())
    showRuntimeError(RuntimeError, Function);
}

void
SourceViewerPanel::showRuntimeError(seec::cm::RuntimeErrorState const &Error,
                                    seec::cm::FunctionState const &InFunction)
{
  // Generate a localised textual description of the error.
  auto MaybeDesc = Error.getDescription();
  
  if (MaybeDesc.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    
    auto const Str = MaybeDesc.get<seec::Error>().getMessage(Status, Locale());
    
    if (U_SUCCESS(Status)) {
      wxLogDebug("Error getting runtime error description: %s.",
                 seec::towxString(Str));
    }
    
    return;
  }
  
  seec::runtime_errors::DescriptionPrinterUnicode Printer { MaybeDesc.move<0>(),
                                                            "\n",
                                                            " " };
  
  // Find the source location of the Stmt that caused the error.
  auto const Statement = Error.getStmt();
  if (!Statement)
    return;
  
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
  
  Panel->annotateLine(Range.EndLine - 1,
                      /* Column */ 0,
                      Printer.getString(),
                      SciLexerType::SeeCRuntimeError,
                      WrapStyle::Wrapped);
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
                        UnicodeString::fromUTF8(Value->getValueAsStringFull()),
                        SciLexerType::SeeCRuntimeValue,
                        WrapStyle::None);
  }
  
  // Show an explanation for the Stmt.
  ExplanationCtrl->showExplanation(Statement, InFunction);
}

void
SourceViewerPanel::showActiveDecl(::clang::Decl const *Declaration,
                                  ::seec::cm::FunctionState const &InFunction)
{
  auto const MappedAST = InFunction.getMappedAST();
  if (!MappedAST)
    return;
  
  auto &ASTUnit = MappedAST->getASTUnit();
  
  auto const Range = getRangeOutermost(Declaration, ASTUnit.getASTContext());
  
  if (!Range.File) {
    wxLogDebug("Couldn't find file for Decl.");
    return;
  }
  
  auto const Panel = loadAndShowFile(Range.File, *MappedAST);
  if (!Panel) {
    wxLogDebug("Couldn't show source panel for file %s.",
               Range.File->getName());
    return;
  }
  
  // Show that the Decl is active.
  Panel->stateIndicatorAdd(SciIndicatorType::CodeActive,
                           Range.Start,
                           Range.End);
  
  // Show an explanation for the Decl.
  ExplanationCtrl->showExplanation(Declaration);
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
