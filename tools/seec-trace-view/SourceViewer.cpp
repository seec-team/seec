#include "SourceViewer.hpp"

#include "llvm/Support/raw_ostream.h"

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
