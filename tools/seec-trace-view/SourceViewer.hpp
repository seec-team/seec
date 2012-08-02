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
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <map>

class SourceViewerPanel : public wxPanel
{
  /// Notebook that holds all of the source windows.
  wxAuiNotebook *Notebook;

  /// Lookup from file path to source window.
  std::map<llvm::sys::Path, wxWindow *> Pages;

public:
  /// Construct without creating.
  SourceViewerPanel()
  : wxPanel()
  {}

  /// Construct and create.
  SourceViewerPanel(wxWindow *Parent,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    Notebook(nullptr),
    Pages()
  {
    Create(Parent, ID, Position, Size);
  }

  /// Destructor.
  virtual ~SourceViewerPanel();

  /// Create the panel.
  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// Remove all files from the viewer.
  void clear();

  /// Add a source file to the viewer, if it doesn't already exist.
  void addSourceFile(llvm::sys::Path FilePath);

  /// Show the file in the viewer (if it exists).
  bool showSourceFile(llvm::sys::Path FilePath);
};

#endif // SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
