//===- SourceViewer.hpp ---------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
#define SEEC_TRACE_VIEW_SOURCEVIEWER_HPP

#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>

#include <map>

class SourceViewerPanel : public wxPanel
{
  /// Notebook that holds all of the source windows.
  wxAuiNotebook *Notebook;
  
  /// Lookup from file path to source window.
  std::map<llvm::sys::Path, wxWindow *> Pages;
  
public:
  SourceViewerPanel(wxWindow *Parent,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : wxPanel(Parent, ID, Position, Size),
    Notebook(new wxAuiNotebook(this,
                               wxID_ANY,
                               wxDefaultPosition,
                               wxDefaultSize,
                               wxAUI_NB_TOP
                               | wxAUI_NB_TAB_SPLIT
                               | wxAUI_NB_TAB_MOVE
                               | wxAUI_NB_SCROLL_BUTTONS))
  {
    auto TopSizer = new wxGridSizer(1, 1, wxSize(0,0));
    TopSizer->Add(Notebook, wxSizerFlags().Expand());
    SetSizerAndFit(TopSizer);
  }
  
  /// Remove all files from the viewer.
  void clear();
  
  /// Add a source file to the viewer, if it doesn't already exist.
  void addSourceFile(llvm::sys::Path FilePath);
  
  /// Show the file in the viewer (if it exists).
  bool showSourceFile(llvm::sys::Path FilePath);
};

#endif // SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
