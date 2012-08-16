#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
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

/// \brief
///
class SourceFilePanel : public wxPanel {
  /// Path to the file.
  llvm::sys::Path FilePath;

  /// Text control that displays the file.
  wxStyledTextCtrl *Text;

  /// Current highlight start.
  long HighlightStart;

  /// Current highlight end.
  long HighlightEnd;

  /// Current annotation line.
  long AnnotationLine;
  
  /// \brief Setup the Scintilla preferences.
  void setSTCPreferences() {
    // Set the lexer to C++.
    Text->SetLexer(wxSTC_LEX_CPP);
        
    // Setup the default common style settings.
    for (auto Type : getAllSciCommonTypes()) {
      auto MaybeStyle = getDefaultStyle(Type);
      if (!MaybeStyle.assigned())
        continue;
      
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
      if (!MaybeStyle.assigned())
        continue;
      
      auto StyleNum = static_cast<int>(Type);
      auto &Style = MaybeStyle.get<0>();
      
      auto Font = Style.Font;
      
      Text->StyleSetForeground(StyleNum, Style.Foreground);
      Text->StyleSetBackground(StyleNum, Style.Background);
      Text->StyleSetFont(StyleNum, Font);
      Text->StyleSetVisible(StyleNum, true);
      Text->StyleSetCase(StyleNum, Style.CaseForce);
    }
    
    // Setup the keywords used by the lexer.
    // TODO: Read keywords from settings, which should read it from the ICU
    // resource bundle.
    Text->SetKeyWords(0,
    wxString("asm auto bool break case catch char class const const_cast "
    "continue default delete do double dynamic_cast else enum explicit "
    "export extern false float for friend goto if inline int long "
    "mutable namespace new operator private protected public register "
    "reinterpret_cast return short signed sizeof static static_cast "
    "struct switch template this throw true try typedef typeid "
    "typename union unsigned using virtual void volatile wchar_t "
    "while"));
    
    Text->SetKeyWords(1, wxString("file"));
    
    Text->SetKeyWords(2,
    wxString("a addindex addtogroup anchor arg attention author b brief bug c "
    "class code date def defgroup deprecated dontinclude e em endcode "
    "endhtmlonly endif endlatexonly endlink endverbatim enum example "
    "exception f$ f[ f] file fn hideinitializer htmlinclude "
    "htmlonly if image include ingroup internal invariant interface "
    "latexonly li line link mainpage name namespace nosubgrouping note "
    "overload p page par param post pre ref relates remarks return "
    "retval sa section see showinitializer since skip skipline struct "
    "subsection test throw todo typedef union until var verbatim "
    "verbinclude version warning weakgroup $ @ \"\" & < > # { }"));
    
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
  // Construct without creating.
  SourceFilePanel()
  : wxPanel(),
    FilePath(),
    Text(nullptr),
    HighlightStart(-1),
    HighlightEnd(-1),
    AnnotationLine(-1)
  {}

  // Construct and create.
  SourceFilePanel(wxWindow *Parent,
                  llvm::sys::Path File,
                  wxWindowID ID = wxID_ANY,
                  wxPoint const &Position = wxDefaultPosition,
                  wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    FilePath(),
    Text(nullptr),
    HighlightStart(-1),
    HighlightEnd(-1),
    AnnotationLine(-1)
  {
    Create(Parent, File, ID, Position, Size);
  }

  /// Destructor.
  virtual ~SourceFilePanel() {}

  /// Create the panel.
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
  void clearState() {
    // Remove existing highlights.
    
    // Remove annotations.
    Text->AnnotationClearAll();
    AnnotationLine = -1;
  }

  ///
  void setHighlight(long StartLine,
                    long StartColumn,
                    long EndLine,
                    long EndColumn) {
    // Remove existing highlight.
    if (HighlightStart != -1 && HighlightEnd != -1) {
      // Text->SetStyle(HighlightStart, HighlightEnd, Text->GetDefaultStyle());
    }

    // Find the position for the new highlight.
    // wxTextCtrl line and column numbers are zero-based, whereas Clang's line
    // and column information is 1-based.
    HighlightStart = Text->XYToPosition(StartColumn - 1, StartLine - 1);
    HighlightEnd = Text->XYToPosition(EndColumn - 1, EndLine - 1);

    if (HighlightStart == -1 || HighlightEnd == -1) {
      wxLogDebug("Couldn't get position information.");
      return;
    }

    Text->SetSelection(HighlightStart, HighlightEnd);
  }

  ///
  void annotateLine(long Line, wxString const &AnnotationText) {
    assert(AnnotationLine == -1);
    
    Text->AnnotationSetText(Line, AnnotationText);
    Text->AnnotationSetVisible(1);
    
    AnnotationLine = Line;
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

          highlightInstruction(Instruction);
        }
        break;
    }

    // We found the last modifier, so stop searching.
    break;
  }
}

void SourceViewerPanel::show(seec::trace::ProcessState const &ProcessState,
                             seec::trace::ThreadState const &ThreadState) {
  // TODO: Clear existing highlights.

  // Find the active function.
  auto &CallStack = ThreadState.getCallStack();
  if (CallStack.empty())
    return;

  auto &FunctionState = CallStack.back();

  auto MaybeInstructionIndex = FunctionState.getActiveInstruction();

  if (MaybeInstructionIndex.assigned()) {
    auto FunctionIndex = FunctionState.getIndex();
    auto InstructionIndex = MaybeInstructionIndex.get<0>();

    auto Lookup = Trace->getModuleIndex().getFunctionIndex(FunctionIndex);
    assert(Lookup && "Couldn't find FunctionIndex.");

    auto Instruction = Lookup->getInstruction(InstructionIndex);
    assert(Instruction && "Couldn't find Instruction.");

    highlightInstruction(Instruction);
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

  It->second->setHighlight(Start.getLine(), Start.getColumn(),
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
                                                  "SourceView_FunctionEntry"));
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

  It->second->setHighlight(Start.getLine(), Start.getColumn(),
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
                                                  "SourceView_FunctionExit"));
}

void SourceViewerPanel::highlightInstruction(llvm::Instruction *Instruction) {
  assert(Trace);

  auto &ClangMap = Trace->getMappedModule();

  // If the Instruction has a mapping to a clang::Stmt, highlight the Stmt.
  auto StmtAndAST = ClangMap.getStmtAndMappedAST(Instruction);

  if (StmtAndAST.first && StmtAndAST.second) {
    auto &SourceManager = StmtAndAST.second->getASTUnit().getSourceManager();
    auto Stmt = StmtAndAST.first;

    auto SpellStart = SourceManager.getSpellingLoc(Stmt->getLocStart());
    auto SpellEnd = SourceManager.getSpellingLoc(Stmt->getLocEnd());

    llvm::sys::Path FilePath(SourceManager.getFilename(SpellStart));

    auto It = Pages.find(FilePath);
    if (It == Pages.end()) {
      wxLogDebug("Couldn't find page for file %s\n",
                 SourceManager.getFilename(SpellStart).str().c_str());
      return;
    }

    // TODO: Clear highlight on current source file.

    auto Index = Notebook->GetPageIndex(It->second);
    Notebook->SetSelection(Index);

    bool Invalid = false;
    auto StartLine = SourceManager.getSpellingLineNumber(SpellStart, &Invalid);
    auto StartCol = SourceManager.getSpellingColumnNumber(SpellStart, &Invalid);
    auto EndLine = SourceManager.getSpellingLineNumber(SpellEnd, &Invalid);
    auto EndCol = SourceManager.getSpellingColumnNumber(SpellEnd, &Invalid);

    if (Invalid) {
      wxLogDebug("Invalid spelling location?");
      return;
    }

    wxLogDebug("Stmt %s %u:%u -> %u:%u",
               Stmt->getStmtClassName(),
               StartLine, StartCol, EndLine, EndCol);

    It->second->setHighlight(StartLine, StartCol, EndLine, EndCol + 1);

    return;
  }

  // Otherwise, if the Instruction has a mapping to a clang::Decl, highlight
  // the Decl.
  auto DeclAndAST = ClangMap.getDeclAndMappedAST(Instruction);

  if (DeclAndAST.first && DeclAndAST.second) {
    auto &SourceManager = DeclAndAST.second->getASTUnit().getSourceManager();
    auto Decl = DeclAndAST.first;

    auto SpellStart = SourceManager.getSpellingLoc(Decl->getLocStart());
    auto SpellEnd = SourceManager.getSpellingLoc(Decl->getLocEnd());

    llvm::sys::Path FilePath(SourceManager.getFilename(SpellStart));

    auto It = Pages.find(FilePath);
    if (It == Pages.end()) {
      wxLogDebug("Couldn't find page for file %s\n",
                 SourceManager.getFilename(SpellStart).str().c_str());
      return;
    }

    // TODO: Clear highlight on current source file.

    auto Index = Notebook->GetPageIndex(It->second);
    Notebook->SetSelection(Index);

    bool Invalid = false;
    auto StartLine = SourceManager.getSpellingLineNumber(SpellStart, &Invalid);
    auto StartCol = SourceManager.getSpellingColumnNumber(SpellStart, &Invalid);
    auto EndLine = SourceManager.getSpellingLineNumber(SpellEnd, &Invalid);
    auto EndCol = SourceManager.getSpellingColumnNumber(SpellEnd, &Invalid);

    if (Invalid) {
      wxLogDebug("Invalid spelling location?");
      return;
    }

    wxLogDebug("Decl %s %u:%u -> %u:%u",
               Decl->getDeclKindName(),
               StartLine, StartCol, EndLine, EndCol);

    It->second->setHighlight(StartLine, StartCol, EndLine, EndCol + 1);

    return;
  }

  // No mapping information was found for this instruction.
  std::string InstructionString;
  {
    // The stream will flush when it is destructed.
    llvm::raw_string_ostream InstructionStream(InstructionString);
    InstructionStream << *Instruction;
  }
  wxLogDebug("No mapping for '%s'", InstructionString.c_str());
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
