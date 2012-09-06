#include "seec/Clang/StateMapping.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/wxWidgets/StringConversion.hpp"
#include "seec/Util/Printing.hpp"

#include "llvm/Instructions.h"
#include "llvm/ADT/ArrayRef.h"

#include <wx/collpane.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "FunctionStateViewer.hpp"
#include "OpenTrace.hpp"


//------------------------------------------------------------------------------
// FunctionStateViewerPanel
//------------------------------------------------------------------------------

FunctionStateViewerPanel::~FunctionStateViewerPanel() {}

bool FunctionStateViewerPanel::Create(wxWindow *Parent,
                                      OpenTrace const &TheTrace,
                                      seec::trace::FunctionState const &State,
                                      wxWindowID ID,
                                      wxPoint const &Position,
                                      wxSize const &Size) {
  if (!wxPanel::Create(Parent,
                       ID,
                       Position,
                       Size))
    return false;
  
  Trace = &TheTrace;
  auto &ClangMap = Trace->getMappedModule();
  
  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));
  
  // Set the label for this panel to the function's name.
  wxString NewLabel;
  
  auto Index = State.getTrace().getIndex();
  auto Function = Trace->getModuleIndex().getFunction(Index);
  if (Function) {
    auto Decl = Trace->getMappedModule().getDecl(Function);
    if (Decl) {
      auto NamedDecl = llvm::dyn_cast<clang::NamedDecl>(Decl);
      assert(NamedDecl);
      NewLabel = wxString(NamedDecl->getNameAsString());
    }
  }
  
  auto ContainerSizer = new wxBoxSizer(wxHORIZONTAL);
  
  // Add the standard contents.
  Container = new wxStaticBoxSizer(wxVERTICAL, this, NewLabel);
  auto StaticBox = Container->GetStaticBox();
  
  auto AllocasStr = new wxStaticText(StaticBox, wxID_ANY,
                                     wxString("Local variables:"));
  Container->Add(AllocasStr, wxSizerFlags());
  
  // Show the state of all Allocas.
  for (auto &Alloca : State.getAllocas()) {
    auto AllocaInst = Alloca.getInstruction();
    
    auto Mapping = ClangMap.getMapping(AllocaInst);
    if (!Mapping.getAST())
      continue;
    
    auto Decl = Mapping.getDecl();
    if (!Decl)
      continue;
    
    auto Value = llvm::dyn_cast<clang::ValueDecl>(Decl);
    if (!Value) {
      wxLogDebug("Decl for AllocaInst is not a ValueDecl");
      continue;
    }
    
    auto Memory = Alloca.getMemoryRegion();
    
    wxString AllocaStr;
    AllocaStr << Value->getType().getAsString()
              << ' '
              << Value->getNameAsString()
              << " = ";
    
    if (Memory.isCompletelyInitialized()) {
      auto const Bytes = Memory.getByteValues();
      AllocaStr << seec::seec_clang::toString(Value,
                                              llvm::ArrayRef<char>(Bytes));
      AllocaStr << ";";
    }
    else {
      AllocaStr << "uninitialized;";
    }
    
    auto AllocaText = new wxStaticText(StaticBox, wxID_ANY, AllocaStr);
    Container->Add(AllocaText, wxSizerFlags());
  }
  
  Container->SetMinSize(wxSize(50, 10));
  
  ContainerSizer->Add(Container, wxSizerFlags().Proportion(1));
  SetSizerAndFit(ContainerSizer);
  
  return true;
}
