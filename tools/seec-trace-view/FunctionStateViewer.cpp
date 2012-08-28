#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

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
                                      wxWindowID ID,
                                      wxPoint const &Position,
                                      wxSize const &Size) {
  if (!wxPanel::Create(Parent,
                       ID,
                       Position,
                       Size))
    return false;
  
  Trace = &TheTrace;
  
  // Add the standard contents.
  auto ContentsSizer = new wxBoxSizer(wxVERTICAL);
  
  Title = new wxStaticText(this, wxID_ANY, wxEmptyString);
  
  ContentsSizer->Add(Title, wxSizerFlags().Proportion(1).Expand());
  
  SetSizerAndFit(ContentsSizer);
  
  return true;
}

void
FunctionStateViewerPanel::showState(seec::trace::FunctionState const &State) {
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
  
  Title->SetLabelText(NewLabel);
}
