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

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <map>


// Forward declarations.

class OpenTrace;
class SourceFilePanel;

namespace seec {
  namespace seec_clang {
    struct SimpleRange;
  }
  namespace trace {
    class ProcessState;
    class RuntimeValue;
    class ThreadState;
  } // namespace trace (in seec)
} // namespace seec

namespace llvm {
  class Function;
  class Instruction;
  class Module;
} // namespace llvm

namespace clang {
  class Stmt;
} // namespace clang


/// \brief SourceViewerPanel.
///
class SourceViewerPanel : public wxPanel
{
  /// Notebook that holds all of the source windows.
  wxAuiNotebook *Notebook;

  /// The currently associated trace information.
  OpenTrace const *Trace;

  /// Lookup from file path to source window.
  std::map<llvm::sys::Path, SourceFilePanel *> Pages;
  
public:
  /// Construct without creating.
  SourceViewerPanel()
  : wxPanel(),
    Notebook(nullptr),
    Trace(nullptr),
    Pages()
  {}

  /// Construct and create.
  SourceViewerPanel(wxWindow *Parent,
                    OpenTrace const &TheTrace,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    Notebook(nullptr),
    Trace(nullptr),
    Pages()
  {
    Create(Parent, TheTrace, ID, Position, Size);
  }

  /// Destructor.
  virtual ~SourceViewerPanel();

  /// Create the panel.
  bool Create(wxWindow *Parent,
              OpenTrace const &TheTrace,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// Remove all files from the viewer.
  void clear();
  
  /// Display and return the SourceFilePanel for the given file.
  SourceFilePanel *showPageForFile(llvm::sys::Path const &File);

  /// Show the current state, without thread-specific information.
  void show(seec::trace::ProcessState const &State);

  /// Show the current state and display thread-specific information for the
  /// given thread.
  void show(seec::trace::ProcessState const &ProcessState,
            seec::trace::ThreadState const &ThreadState);
  
private:
  /// \name State display.
  /// @{
  
  /// Highlight the source code associated with entering the specified Function.
  void highlightFunctionEntry(llvm::Function *Function);

  /// Highlight the source code associated with exiting the specified Function.
  void highlightFunctionExit(llvm::Function *Function);
  
  ///
  void showActiveRange(SourceFilePanel *Page,
                       seec::seec_clang::SimpleRange const &Range);
  
  ///
  void showActiveStmt(::clang::Stmt const *Statement,
                      seec::seec_clang::MappedAST const &AST,
                      llvm::StringRef Value);

  /// Highlight the source code associated with the specified Instruction.
  void highlightInstruction(llvm::Instruction const *Instruction,
                            seec::trace::RuntimeValue const &Value,
                            seec::runtime_errors::RunError const *Error);

  /// @}
  
  
  /// Add a source file to the viewer, if it doesn't already exist.
  void addSourceFile(llvm::sys::Path FilePath);

  /// Show the file in the viewer (if it exists).
  bool showSourceFile(llvm::sys::Path FilePath);
};

#endif // SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
