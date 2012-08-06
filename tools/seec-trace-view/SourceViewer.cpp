#include "SourceViewer.hpp"
#include "OpenTrace.hpp"

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/TraceSearch.hpp"

#include "llvm/Instruction.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"

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

#if 0
  // TODO: Certain events that modify shared state may not be associated with
  // an Instruction, but with a FunctionEnd. In this case we should still show
  // something meaningful, so we need to change the below to check whether
  // or not the state event is subservient to a FunctionEnd or an Instruction
  // event, and then act accordingly.

  // Find and highlight the last-executed Instruction (if any).
  auto Time = State.getProcessTime();

  for (auto &ThreadState : State.getThreadStates()) {
    auto MaybeModifierRef = ThreadState->getLastProcessModifier();
    if (!MaybeModifierRef.assigned())
      continue;

    auto MaybeTime = MaybeModifierRef.get<0>()->getProcessTime();
    if (!MaybeTime.assigned())
      continue;

    if (MaybeTime.get<0>() != Time)
      continue;

    // This event is responsible for the last shared state modification, now we
    // have to find the llvm::Instruction associated with it.
    auto EvRef = MaybeModifierRef.get<0>();

    // Find the function that contains the event.
    auto const &ThreadTrace = ThreadState->getTrace();
    auto MaybeFunctionTrace = ThreadTrace.getFunctionContaining(EvRef);
    assert(MaybeFunctionTrace.assigned());

    // Get the index of the llvm::Function.
    auto FunctionIndex = MaybeFunctionTrace.get<0>().getIndex();

    // Find the instruction event associated with this event.
    while (!EvRef->isInstruction())
      --EvRef;

    // Get the index of the llvm::Instruction.
    auto MaybeInstructionIndex = EvRef->getIndex();
    assert(MaybeInstructionIndex.assigned());

    auto Lookup = Trace.getModuleIndex().getFunctionIndex(FunctionIndex);
    assert(Lookup);

    auto Instruction = Lookup->getInstruction(MaybeInstructionIndex.get<0>());
    assert(Instruction);

    highlightInstruction(Instruction);
    break;
  }
#endif
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
  llvm::errs() << "highlightInstruction(" << Instruction << ")\n";

  // TODO:
  // - Remove existing highlight.
  // - Get the source file associated with Instruction.
  // - Switch view to the source file (if required).
  // - Get the Decl/Stmt associated with Instruction.
  // - Get the range of the Decl/Stmt.
  // - Highlight the range.
}

void SourceViewerPanel::addSourceFile(llvm::sys::Path FilePath) {
  if (Pages.count(FilePath))
    return;

  auto TextCtrl = new wxTextCtrl(this,
                                 wxID_ANY,
                                 wxEmptyString,
                                 wxDefaultPosition,
                                 wxDefaultSize,
                                 wxTE_MULTILINE
                                 | wxTE_RICH
                                 | wxHSCROLL
                                 | wxTE_READONLY);

  TextCtrl->LoadFile(FilePath.str());

  Notebook->AddPage(TextCtrl, FilePath.c_str());

  Pages.insert(std::make_pair(FilePath,
                              static_cast<wxWindow *>(TextCtrl)));
}

bool SourceViewerPanel::showSourceFile(llvm::sys::Path FilePath) {
  auto It = Pages.find(FilePath);
  if (It == Pages.end())
    return false;

  auto Index = Notebook->GetPageIndex(It->second);
  Notebook->SetSelection(Index);

  return true;
}
