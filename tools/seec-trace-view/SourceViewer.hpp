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

#include "seec/Clang/MappedValue.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <map>
#include <memory>

#include "HighlightEvent.hpp"


// Forward declarations.

class ExplanationViewer;
class OpenTrace;
class SourceFilePanel;

namespace seec {
  namespace seec_clang {
    struct SimpleRange;
  } // namespace seec_clang (in seec)
  namespace trace {
    class FunctionState;
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
  class Decl;
  class Stmt;
} // namespace clang


/// \brief A range in a source file.
///
struct SourceFileRange {
  std::string Filename;
  
  unsigned Start;
  
  unsigned StartLine;
  
  unsigned StartColumn;
  
  unsigned End;
  
  unsigned EndLine;
  
  unsigned EndColumn;
  
  SourceFileRange()
  : Filename(),
    Start(),
    StartLine(),
    StartColumn(),
    End(),
    EndLine(),
    EndColumn()
  {}
  
  SourceFileRange(std::string WithFilename,
                  unsigned WithStart,
                  unsigned WithStartLine,
                  unsigned WithStartColumn,
                  unsigned WithEnd,
                  unsigned WithEndLine,
                  unsigned WithEndColumn)
  : Filename(std::move(WithFilename)),
    Start(WithStart),
    StartLine(WithStartLine),
    StartColumn(WithStartColumn),
    End(WithEnd),
    EndLine(WithEndLine),
    EndColumn(WithEndColumn)
  {}
};


/// \brief SourceViewerPanel.
///
class SourceViewerPanel : public wxPanel
{
  /// Notebook that holds all of the source windows.
  wxAuiNotebook *Notebook;
  
  /// Text control that holds explanatory material.
  ExplanationViewer *ExplanationCtrl;
  
  /// The currently associated trace information.
  OpenTrace const *Trace;

  /// Lookup from file path to source window.
  std::map<llvm::sys::Path, SourceFilePanel *> Pages;
  
public:
  /// \brief Construct without creating.
  SourceViewerPanel()
  : wxPanel(),
    Notebook(nullptr),
    ExplanationCtrl(nullptr),
    Trace(nullptr),
    Pages()
  {}

  /// \brief Construct and create.
  SourceViewerPanel(wxWindow *Parent,
                    OpenTrace const &TheTrace,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    Notebook(nullptr),
    ExplanationCtrl(nullptr),
    Trace(nullptr),
    Pages()
  {
    Create(Parent, TheTrace, ID, Position, Size);
  }

  /// \brief Destructor.
  virtual ~SourceViewerPanel();

  /// \brief Create the panel.
  bool Create(wxWindow *Parent,
              OpenTrace const &TheTrace,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  
  /// \name Mutators.
  /// @{
  
  /// Remove all files from the viewer.
  void clear();
  
  /// Display and return the SourceFilePanel for the given file.
  SourceFilePanel *showPageForFile(llvm::sys::Path const &File);

  /// Show the current state, without thread-specific information.
  void show(seec::trace::ProcessState const &State,
            std::shared_ptr<seec::cm::ValueStore const> ValueStore);

  /// Show the current state and display thread-specific information for the
  /// given thread.
  void show(seec::trace::ProcessState const &ProcessState,
            std::shared_ptr<seec::cm::ValueStore const> ValueStore,
            seec::trace::ThreadState const &ThreadState);

  /// @} (Mutators).
  
  
  /// \name Location lookup.
  /// @{
  
public:  
  SourceFileRange getRange(::clang::Decl const *Decl) const;
  
  SourceFileRange getRange(::clang::Stmt const *Stmt,
                           ::clang::ASTContext const &AST) const;
  
  SourceFileRange getRange(::clang::Stmt const *Stmt) const;
  
  /// @}
  
  
  /// \name Highlighting.
  /// @{
private:
  /// Set highlighting for a Decl.
  void highlightOn(::clang::Decl const *Decl);
  
  /// Set highlighting for a Stmt.
  void highlightOn(::clang::Stmt const *Stmt);
  
  /// Clear all highlighting.
  void highlightOff();
  
public:
  /// Set highlighting for the given event.
  void OnHighlightOn(HighlightEvent const &Ev);
  
  /// Clear highlighting for the given event.
  void OnHighlightOff(HighlightEvent const &Ev);
  
  /// @}
  
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
  void showActiveDecl(::clang::Decl const *Decl,
                      seec::seec_clang::MappedAST const &AST);
  
  ///
  void showActiveStmt(::clang::Stmt const *Statement,
                      seec::seec_clang::MappedAST const &AST,
                      seec::trace::FunctionState const &FunctionState,
                      std::shared_ptr<seec::cm::ValueStore const> ValueStore,
                      llvm::StringRef Value,
                      wxString const &Error);

  /// Highlight the source code associated with the specified Instruction.
  void
  highlightInstruction(llvm::Instruction const *Instruction,
                       seec::trace::RuntimeValue const &Value,
                       seec::runtime_errors::RunError const *Error,
                       seec::trace::FunctionState const &FunctionState,
                       std::shared_ptr<seec::cm::ValueStore const> ValueStore);

  /// @}
  
  
  /// Add a source file to the viewer, if it doesn't already exist.
  void addSourceFile(llvm::sys::Path FilePath);

  /// Show the file in the viewer (if it exists).
  bool showSourceFile(llvm::sys::Path FilePath);
};

#endif // SEEC_TRACE_VIEW_SOURCEVIEWER_HPP
