#include "seec/Clang/SourceMapping.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Function.h"
#include "llvm/Instruction.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <wx/font.h>
#include <wx/stc/stc.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "SourceViewer.hpp"
#include "SourceViewerSettings.hpp"
#include "OpenTrace.hpp"

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
  
  
  /// \brief Setup the Scintilla preferences.
  ///
  void setSTCPreferences() {
    // Set the lexer to C++.
    Text->SetLexer(wxSTC_LEX_CPP);
        
    // Setup the default common style settings.
    for (auto Type : getAllSciCommonTypes()) {
      auto MaybeStyle = getDefaultStyle(Type);
      if (!MaybeStyle.assigned()) {
        wxLogDebug("Couldn't get default style for style %s",
                   getSciTypeName(Type));
        
        continue;
      }
      
      auto StyleNum = static_cast<int>(Type);
      auto &Style = MaybeStyle.get<0>();
      
      auto Font = Style.Font;
      
      Text->StyleSetForeground(StyleNum, Style.Foreground);
      Text->StyleSetBackground(StyleNum, Style.Background);
      Text->StyleSetFont(StyleNum, Font);
      Text->StyleSetVisible(StyleNum, true);
      Text->StyleSetCase(StyleNum, Style.CaseForce);
    }
    
    // Setup the default style settings for the lexer.
    for (auto Type : getAllSciLexerTypes()) {
      auto MaybeStyle = getDefaultStyle(Type);
      if (!MaybeStyle.assigned()) {
        wxLogDebug("Couldn't get default style for lexer style %s",
                   getSciTypeName(Type));
        
        continue;
      }
      
      auto StyleNum = static_cast<int>(Type);
      auto &Style = MaybeStyle.get<0>();
      
      auto Font = Style.Font;
      
      Text->StyleSetForeground(StyleNum, Style.Foreground);
      Text->StyleSetBackground(StyleNum, Style.Background);
      Text->StyleSetFont(StyleNum, Font);
      Text->StyleSetVisible(StyleNum, true);
      Text->StyleSetCase(StyleNum, Style.CaseForce);
    }
    
    // Setup the style settings for our indicators.
    for (auto Type : getAllSciIndicatorTypes()) {
      auto MaybeStyle = getDefaultIndicatorStyle(Type);
      if (!MaybeStyle.assigned()) {
        wxLogDebug("Couldn't get default style for indicator %s",
                   getSciIndicatorTypeName(Type));
        
        continue;
      }
      
      auto Indicator = static_cast<int>(Type);
      auto &IndicatorStyle = MaybeStyle.get<0>();
      
      Text->IndicatorSetStyle(Indicator, IndicatorStyle.Style);
      Text->IndicatorSetForeground(Indicator, IndicatorStyle.Foreground);
      Text->IndicatorSetAlpha(Indicator, IndicatorStyle.Alpha);
      Text->IndicatorSetOutlineAlpha(Indicator, IndicatorStyle.OutlineAlpha);
      Text->IndicatorSetUnder(Indicator, IndicatorStyle.Under);
    }
    
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
    
    // Miscellaneous.
    Text->SetIndentationGuides(true);
    Text->SetEdgeColumn(80);
    // Text->SetEdgeMode(wxSTC_EDGE_LINE);
    Text->SetWrapMode(wxSTC_WRAP_NONE);
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
  // \brief Construct without creating.
  SourceFilePanel()
  : wxPanel(),
    FilePath(),
    Text(nullptr),
    StateIndications(),
    StateAnnotations()
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
    StateAnnotations()
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
    
    if (Start == -1 || End == -1) {
      wxLogDebug("SourceFilePanel::setStateIndicator couldn't find position"
                 " information!");
      
      return false;
    }
    
    // Set the indicator on the text. Clang's source ranges have an inclusive
    // end, whereas Scintilla's is exclusive, hence the (End + 1).
    int IndicatorValue = static_cast<int>(Indicator);
    int Length = (End + 1) - Start;
    
    Text->SetIndicatorCurrent(IndicatorValue);
    Text->IndicatorFillRange(Start, Length);
    
    // Save the indicator so that we can clear it in clearState().
    StateIndications.emplace_back(IndicatorValue, Start, Length);
    
    return true;
  }

  /// \brief Annotate a line for this state.
  ///
  void annotateLine(long Line,
                    wxString const &AnnotationText,
                    SciLexerType AnnotationStyle) {
    Text->AnnotationSetText(Line, AnnotationText);
    Text->AnnotationSetVisible(1);
    Text->AnnotationSetStyle(Line, static_cast<int>(AnnotationStyle));
    StateAnnotations.push_back(Line);
  }
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

  auto TopSizer = new wxGridSizer(1, 1, wxSize(0,0));
  TopSizer->Add(Notebook, wxSizerFlags().Expand());
  SetSizerAndFit(TopSizer);

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

          highlightInstruction(Instruction, nullptr);
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
    highlightInstruction(Instruction, RuntimeError);
  }
  else {
    // If there is no active Instruction, highlight the function entry.
    auto FunctionIndex = FunctionState.getIndex();
    auto Func = Trace->getModuleIndex().getFunction(FunctionIndex);
    assert(Func && "Couldn't find llvm::Function.");
    
    highlightFunctionEntry(Func);
  }
}

void SourceViewerPanel::highlightFunctionEntry(llvm::Function *Function) {
  auto Mapping = Trace->getMappedModule().getMappedGlobalDecl(Function);
  if (!Mapping) {
    wxLogDebug("No mapping for Function '%s'",
               Function->getName().str().c_str());
    return;
  }

  auto Decl = Mapping->getDecl();
  auto &SourceManager = Mapping->getAST().getASTUnit().getSourceManager();

  auto Start = SourceManager.getPresumedLoc(Decl->getLocStart());
  auto End = SourceManager.getPresumedLoc(Decl->getLocEnd());

  if (strcmp(Start.getFilename(), End.getFilename())) {
    wxLogDebug("Don't know how to highlight Stmt across files: %s and %s\n",
               Start.getFilename(),
               End.getFilename());
    return;
  }

  llvm::sys::Path FilePath(Start.getFilename());

  auto It = Pages.find(FilePath);
  if (It == Pages.end()) {
    wxLogDebug("Couldn't find page for file %s\n", Start.getFilename());
    return;
  }

  // TODO: Clear highlight on current source file?

  wxLogDebug("Setting highlight on file %s\n", Start.getFilename());

  auto Index = Notebook->GetPageIndex(It->second);
  Notebook->SetSelection(Index);

  It->second->setStateIndicator(SciIndicatorType::ActiveCode,
                                Start.getLine(), Start.getColumn(),
                                End.getLine(), End.getColumn() + 1);

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  It->second->annotateLine(Start.getLine() - 1,
                           seec::getwxStringExOrEmpty(
                                                  TextTable,
                                                  "SourceView_FunctionEntry"),
                           SciLexerType::SeeCRuntimeInformation);
}

void SourceViewerPanel::highlightFunctionExit(llvm::Function *Function) {
  auto Mapping = Trace->getMappedModule().getMappedGlobalDecl(Function);
  if (!Mapping) {
    wxLogDebug("No mapping for Function '%s'",
               Function->getName().str().c_str());
    return;
  }

  auto Decl = Mapping->getDecl();
  auto &SourceManager = Mapping->getAST().getASTUnit().getSourceManager();

  auto Start = SourceManager.getPresumedLoc(Decl->getLocStart());
  auto End = SourceManager.getPresumedLoc(Decl->getLocEnd());

  if (strcmp(Start.getFilename(), End.getFilename())) {
    wxLogDebug("Don't know how to highlight Stmt across files: %s and %s\n",
               Start.getFilename(),
               End.getFilename());
    return;
  }

  llvm::sys::Path FilePath(Start.getFilename());

  auto It = Pages.find(FilePath);
  if (It == Pages.end()) {
    wxLogDebug("Couldn't find page for file %s\n", Start.getFilename());
    return;
  }

  // TODO: Clear highlight on current source file?

  wxLogDebug("Setting highlight on file %s\n", Start.getFilename());

  auto Index = Notebook->GetPageIndex(It->second);
  Notebook->SetSelection(Index);

  It->second->setStateIndicator(SciIndicatorType::ActiveCode,
                                Start.getLine(), Start.getColumn(),
                                End.getLine(), End.getColumn() + 1);

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));

  It->second->annotateLine(Start.getLine() - 1,
                           seec::getwxStringExOrEmpty(
                                                  TextTable,
                                                  "SourceView_FunctionExit"),
                           SciLexerType::SeeCRuntimeInformation);
}

void
SourceViewerPanel::showInstructionAt
  ( llvm::Instruction const *Instruction,
    SourceFilePanel *Page,
    seec::seec_clang::SimpleRange const &Range
  ) {
  Page->setStateIndicator(SciIndicatorType::ActiveCode,
                          Range.Start.Line, Range.Start.Column,
                          Range.End.Line, Range.End.Column);
}

void SourceViewerPanel::highlightInstruction
      (llvm::Instruction const *Instruction,
       seec::runtime_errors::RunError const *Error) {
  assert(Trace);

  auto &ClangMap = Trace->getMappedModule();
  
  auto InstructionMap = ClangMap.getMapping(Instruction);
  
  if (!InstructionMap.getAST()) {
    // No mapping information was found for this instruction.
    std::string InstructionString;
    
    {
      // The stream will flush when it is destructed.
      llvm::raw_string_ostream InstructionStream(InstructionString);
      InstructionStream << *Instruction;
    }
    
    wxLogDebug("No mapping for '%s'", InstructionString.c_str());
    
    return;
  }
  
  auto &AST = InstructionMap.getAST()->getASTUnit();
  
  // TODO: Clear state mapping on the current source file.
  
  // Focus on the mapped source file.
  auto PageIt = Pages.find(InstructionMap.getFilePath());
  if (PageIt == Pages.end()) {
    wxLogDebug("Couldn't find page for file.\n");
    return;
  }
  
  auto PageIndex = Notebook->GetPageIndex(PageIt->second);
  Notebook->SetSelection(PageIndex);
  
  // Now highlight the instruction.
  if (InstructionMap.getStmt()) {
    // Highlight the Stmt.
    auto MaybeRange
      = seec::seec_clang::getPrettyVisibleRange(InstructionMap.getStmt(), AST);
    
    if (MaybeRange.assigned()) {
      showInstructionAt(Instruction, PageIt->second, MaybeRange.get<0>());
      
      // Show a description of the RunError.
      if (Error) {
        auto UniStr = seec::runtime_errors::format(*Error);
        auto ErrorStr = seec::towxString(UniStr);
        PageIt->second->annotateLine(MaybeRange.get<0>().Start.Line - 1,
                                     ErrorStr,
                                     SciLexerType::SeeCRuntimeError);
      }
    }
  }
  else if (InstructionMap.getDecl()) {
    // Highlight the Decl.
    auto MaybeRange
      = seec::seec_clang::getPrettyVisibleRange(InstructionMap.getDecl(), AST);
    
    if (MaybeRange.assigned()) {
      showInstructionAt(Instruction, PageIt->second, MaybeRange.get<0>());
    }    
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
