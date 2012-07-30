#include "SourceViewer.hpp"

#include "llvm/Support/raw_ostream.h"

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
