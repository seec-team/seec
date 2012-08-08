#include "SourceViewer.hpp"
#include "OpenTrace.hpp"

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceSearch.hpp"

#include "llvm/Instruction.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <wx/stc/stc.h>

//------------------------------------------------------------------------------
// SourceWindow
//------------------------------------------------------------------------------

class SourceFilePanel : public wxPanel {
  /// Path to the file.
  llvm::sys::Path FilePath;
  
  /// Text control that displays the file.
  wxStyledTextCtrl *Text;
  
  /// Current highlight start.
  long HighlightStart;
  
  /// Current highlight end.
  long HighlightEnd;
  
public:
  // Construct without creating.
  SourceFilePanel()
  : wxPanel(),
    FilePath(),
    Text(nullptr),
    HighlightStart(-1),
    HighlightEnd(-1)
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
    HighlightEnd(-1)
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
    
    if (!Text->LoadFile(FilePath.str())) {
      // TODO: Insert a localized error message.
    }
    
    Text->SetReadOnly(true);
    
    auto Sizer = new wxBoxSizer(wxHORIZONTAL);
    Sizer->Add(Text, wxSizerFlags().Proportion(1).Expand());
    SetSizerAndFit(Sizer);
    
    return true;
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
    HighlightEnd = Text->XYToPosition(EndColumn - 1, EndLine - 1) + 1;
    
    if (HighlightStart == -1 || HighlightEnd == -1) {
      wxLogDebug("Couldn't get position information.");
      return;
    }
    
    Text->SetSelection(HighlightStart, HighlightEnd);
  }
};


//------------------------------------------------------------------------------
// SourceViewerPanel
//------------------------------------------------------------------------------

SourceViewerPanel::~SourceViewerPanel() {}

bool SourceViewerPanel::Create(wxWindow *Parent,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;

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

  return true;
}

void SourceViewerPanel::clear() {
  Notebook->DeleteAllPages();
  Pages.clear();
}

void SourceViewerPanel::show(OpenTrace const &Trace,
                             seec::trace::ProcessState const &State) {
  if (&Trace != this->Trace)
    setTrace(&Trace);

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
        // TODO: Highlight the function entry.
        {
          wxLogDebug("Highlight FunctionStart not implemented.");
        }
        break;
        
      case seec::trace::EventType::FunctionEnd:
        // TODO: Highlight the function exit.
        {
          wxLogDebug("Highlight FunctionEnd not implemented.");
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
          
          auto Lookup = Trace.getModuleIndex().getFunctionIndex(FunctionIndex);
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

void SourceViewerPanel::setTrace(OpenTrace const *Trace) {
  if (Trace == this->Trace)
    return;

  clear();

  this->Trace = Trace;

  if (!Trace)
    return;

  // Load all source files.
  for (auto &MapGlobalPair : Trace->getMappedModule().getGlobalLookup()) {
    addSourceFile(MapGlobalPair.second.getFilePath());
  }
}

void SourceViewerPanel::highlightInstruction(llvm::Instruction *Instruction) {
  assert(Trace);
  
  wxLogDebug("highlightInstruction(%p)\n", Instruction);
  
  auto &ClangMap = Trace->getMappedModule();
  
  // If the Instruction has a mapping to a clang::Stmt, highlight the Stmt.
  auto StmtAndAST = ClangMap.getStmtAndMappedAST(Instruction);
  
  if (StmtAndAST.first && StmtAndAST.second) {
    wxLogDebug("Stmt found @%p\n", StmtAndAST.first);
    
    auto &SourceManager = StmtAndAST.second->getASTUnit().getSourceManager();
    auto Start = SourceManager.getPresumedLoc(StmtAndAST.first->getLocStart());
    auto End = SourceManager.getPresumedLoc(StmtAndAST.first->getLocEnd());
    
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
                             End.getLine(), End.getColumn());
    
    return;
  }
  
  // Otherwise, if the Instruction has a mapping to a clang::Decl, highlight
  // the Decl.
  auto DeclAndAST = ClangMap.getDeclAndMappedAST(Instruction);
  
  if (DeclAndAST.first && DeclAndAST.second) {
    wxLogDebug("Decl found @%p\n", DeclAndAST.first);
    
    auto &SourceManager = DeclAndAST.second->getASTUnit().getSourceManager();
    auto Start = SourceManager.getPresumedLoc(DeclAndAST.first->getLocStart());
    auto End = SourceManager.getPresumedLoc(DeclAndAST.first->getLocEnd());
    
    if (strcmp(Start.getFilename(), End.getFilename())) {
      wxLogDebug("Don't know how to highlight Decl across files: %s and %s\n",
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
                             End.getLine(), End.getColumn());
    
    return;
  }
  
  // No mapping information was found for this instruction.
  std::string InstructionString;
  {
    // The stream will flush when it is destructed.
    llvm::raw_string_ostream InstructionStream(InstructionString);
    InstructionStream << *Instruction;
  }
  wxLogDebug("No mapping for '%s'\n", InstructionString.c_str());
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
