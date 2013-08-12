//===- tools/seec-trace-view/SourceViewer.hpp -----------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
#define SEEC_TRACE_VIEW_SOURCEVIEWER_HPP

#include "llvm/ADT/StringRef.h"

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <map>
#include <memory>


// Forward declarations.

class ContextNotifier;
class ExplanationViewer;
class OpenTrace;
class SourceFilePanel;
class StateAccessToken;

namespace seec {
  namespace cm {
    class FunctionState;
    class ProcessState;
    class RuntimeErrorState;
    class ThreadState;
  }
  namespace seec_clang {
    class MappedAST;
  }
}

namespace clang {
  class Decl;
  class FileEntry;
  class Stmt;
}


/// \brief SourceViewerPanel.
///
class SourceViewerPanel : public wxPanel
{
  /// Notebook that holds all of the source windows.
  wxAuiNotebook *Notebook;
  
  /// The currently associated trace information.
  OpenTrace const *Trace;
  
  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// Lookup from file path to source window.
  std::map<clang::FileEntry const *, SourceFilePanel *> Pages;
  
  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  /// Text control that holds explanatory material.
  ExplanationViewer *ExplanationCtrl;
  
public:
  /// \brief Construct without creating.
  ///
  SourceViewerPanel();

  /// \brief Construct and create.
  ///
  SourceViewerPanel(wxWindow *Parent,
                    OpenTrace const &TheTrace,
                    ContextNotifier &WithNotifier,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize);

  /// \brief Destructor.
  ///
  virtual ~SourceViewerPanel();

  /// \brief Create the panel.
  ///
  bool Create(wxWindow *Parent,
              OpenTrace const &TheTrace,
              ContextNotifier &WithNotifier,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  
  /// \name Mutators.
  /// @{
  
  /// \brief Remove all files from the viewer.
  ///
  void clear();
  
  /// \brief Update this panel to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);
  
  /// @} (Mutators).

private:
  /// \name State display.
  /// @{

  /// \brief Show the given runtime error in the source code view.
  ///
  void showRuntimeError(seec::cm::RuntimeErrorState const &Error,
                        seec::cm::FunctionState const &InFunction);
  
  /// \brief Show the given Stmt in the source code view.
  ///
  void showActiveStmt(::clang::Stmt const *Statement,
                      ::seec::cm::FunctionState const &InFunction);
  
  /// \brief Show the given Decl in the source code view.
  ///
  void showActiveDecl(::clang::Decl const *Declaration,
                      ::seec::cm::FunctionState const &InFunction);
  
  /// @} (State display.)
  
  
  /// \name File management.
  /// @{

  /// \brief Show the given source file.
  ///
  SourceFilePanel *loadAndShowFile(clang::FileEntry const *File,
                                   seec::seec_clang::MappedAST const &MAST);
  
  /// @} (File management.)
  

  /// \name Highlighting.
  /// @{

  /// Set highlighting for a Decl.
  void highlightOn(::clang::Decl const *Decl);
  
  /// Set highlighting for a Stmt.
  void highlightOn(::clang::Stmt const *Stmt);
  
  /// Clear all highlighting.
  void highlightOff();
  
  /// @}  
};

#endif // SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
