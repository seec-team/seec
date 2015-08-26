//===- tools/seec-trace-view/ExplanationViewer.hpp ------------------------===//
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

#ifndef SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP
#define SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP

#include "seec/Clang/MappedValue.hpp"
#include "seec/ClangEPV/ClangEPV.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/stc/stc.h>

#include <memory>
#include <string>


// Forward declarations.
namespace clang {
  class Decl;
  class Stmt;
} // namespace clang

namespace seec {
  namespace cm {
    class FunctionState;
    class ProcessState;
  }
}

class ActionRecord;
class ActionReplayFrame;
class ContextNotifier;
class IndexedAnnotationText;
class OpenTrace;
class StateAccessToken;


/// \brief ExplanationViewer.
///
class ExplanationViewer final : public wxStyledTextCtrl
{
  /// The trace that this viewer will display states from.
  OpenTrace *Trace;

  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// Used to record user interactions.
  ActionRecord *Recording;

  /// Holds the current annotation text.
  std::unique_ptr<IndexedAnnotationText> Annotation;

  /// Holds the byte length of displayed annotation text.
  long AnnotationLength;

  /// Hold current explanatory material.
  std::unique_ptr<seec::clang_epv::Explanation> Explanation;
  
  /// Caches the current mouse position.
  int CurrentMousePosition;
  
  /// Currently highlighted Decl.
  ::clang::Decl const *HighlightedDecl;
  
  /// Currently highlighted Stmt.
  ::clang::Stmt const *HighlightedStmt;
  
  /// Is the mouse currently hovering on a URL?
  bool URLHover;
  
  /// The URL that the mouse is hovering over.
  std::string URLHovered;

  /// Is the mouse on the same URL as when the left button was clicked?
  bool URLClick;
  
  /// \brief Get byte offset range from "whole character" range.
  ///
  std::pair<int, int> getAnnotationByteOffsetRange(int32_t Start, int32_t End);

  /// \brief Get byte offset range from "whole character" range.
  ///
  std::pair<int, int> getExplanationByteOffsetRange(int32_t Start, int32_t End);
  
  /// \brief Set the annotation text.
  ///
  void setAnnotationText(wxString const &Value);

  /// \brief Set the explanation text.
  ///
  void setExplanationText(wxString const &Value);
  
  /// \brief Set indicators for the interactive text areas in the current
  ///        \c Explanation.
  ///
  void setExplanationIndicators();

  /// \brief Handle mouse moving over a link to a \c clang::Decl.
  ///
  void mouseOverDecl(clang::Decl const *);

  /// \brief Handle mouse moving over a link to a \c clang::Stmt.
  ///
  void mouseOverStmt(clang::Stmt const *);

  /// \brief Handle mouse moving over a hyperlink.
  ///
  void mouseOverHyperlink(UnicodeString const &);

  /// \brief Clear the current information.
  ///
  void clearCurrent();
  
public:
  /// \brief Construct without creating.
  ///
  ExplanationViewer()
  : wxStyledTextCtrl(),
    Trace(nullptr),
    Notifier(nullptr),
    Recording(nullptr),
    Annotation(nullptr),
    AnnotationLength(0),
    Explanation(),
    CurrentMousePosition(wxSTC_INVALID_POSITION),
    HighlightedDecl(nullptr),
    HighlightedStmt(nullptr),
    URLHover(false),
    URLHovered(),
    URLClick(false)
  {}

  /// \brief Construct and create.
  ///
  ExplanationViewer(wxWindow *Parent,
                    OpenTrace &WithTrace,
                    ContextNotifier &WithNotifier,
                    ActionRecord &WithRecording,
                    ActionReplayFrame &WithReplay,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : ExplanationViewer()
  {
    Create(Parent, WithTrace, WithNotifier, WithRecording, WithReplay, ID,
           Position, Size);
  }

  /// \brief Destructor.
  ///
  virtual ~ExplanationViewer();

  /// \brief Create the viewer.
  ///
  bool Create(wxWindow *Parent,
              OpenTrace &WithTrace,
              ContextNotifier &WithNotifier,
              ActionRecord &WithRecording,
              ActionReplayFrame &WithReplay,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  
  /// \name Mouse events.
  /// @{
  
  void OnMotion(wxMouseEvent &Event);
  
  void OnEnterWindow(wxMouseEvent &Event);
  
  void OnLeaveWindow(wxMouseEvent &Event);
  
  void OnLeftDown(wxMouseEvent &Event);
  
  void OnLeftUp(wxMouseEvent &Event);
  
  /// @} (Mouse events)
  
  
  /// \name Mutators.
  /// @{
  
private:
  /// \brief Show annotations for this state.
  /// \return true iff ClangEPV explanation should be suppressed.
  ///
  bool showAnnotations(seec::cm::ProcessState const &Process,
                       seec::cm::ThreadState const &Thread);

public:
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);
  
  /// \brief Attempt to show an explanation for the given Decl.
  ///
  void showExplanation(::clang::Decl const *Decl);
  
  /// \brief Attempt to show an explanation for the given Stmt.
  ///
  /// pre: Caller must have locked access to the state containing InFunction.
  ///
  void showExplanation(::clang::Stmt const *Statement,
                       ::seec::cm::FunctionState const &InFunction);
  
  /// \brief Clear the display.
  ///
  void clearExplanation();
  
  /// @} (Mutators)
};

#endif // SEEC_TRACE_VIEW_EXPLANATIONVIEWER_HPP
