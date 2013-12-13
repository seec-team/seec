//===- tools/seec-trace-view/StateEvaluationTree.hpp ----------------------===//
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

#ifndef SEEC_TRACE_VIEW_STATEEVALUATIONTREE_HPP
#define SEEC_TRACE_VIEW_STATEEVALUATIONTREE_HPP

#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "llvm/ADT/DenseMap.h"

#include <memory>
#include <string>


namespace seec {
  namespace cm {
    class FunctionState;
    class ProcessState;
    class ThreadState;
    class Value;
  }
}

namespace clang {
  class Stmt;
}

class ContextNotifier;
class StateAccessToken;


/// \brief Displays a collection of state viewers.
///
class StateEvaluationTreePanel final : public wxScrolled<wxPanel>
{
  /// \brief Information for a single node in the tree.
  ///
  struct NodeInfo {
    /// This node's Stmt.
    clang::Stmt const *Statement;
    
    /// Value produced by the evaluation of this node.
    std::shared_ptr<seec::cm::Value const> Value;
    
    /// Start of this node's text in the pretty-printed Stmt.
    uint64_t RangeStart;
    
    /// Length of this node's text in the pretty-printed Stmt.
    std::size_t RangeLength;
    
    /// Depth of this node in the current top-level Stmt.
    unsigned Depth;
    
    /// Left hand side of this node's rectangle.
    wxCoord XStart;
    
    /// Right hand side of this node's rectangle.
    wxCoord XEnd;
    
    /// Top of this node's rectangle.
    wxCoord YStart;
    
    /// End of this node's rectangle.
    wxCoord YEnd;
    
    /// \brief Constructor.
    ///
    NodeInfo(clang::Stmt const *ForStatement,
             std::shared_ptr<seec::cm::Value const> WithValue,
             uint64_t WithRangeStart,
             std::size_t WithRangeLength,
             unsigned WithDepth,
             wxCoord WithXStart,
             wxCoord WithXEnd,
             wxCoord WithYStart,
             wxCoord WithYEnd)
    : Statement(ForStatement),
      Value(WithValue),
      RangeStart(WithRangeStart),
      RangeLength(WithRangeLength),
      Depth(WithDepth),
      XStart(WithXStart),
      XEnd(WithXEnd),
      YStart(WithYStart),
      YEnd(WithYEnd)
    {}
  };
  
  /// \brief Contains settings that control the display of the evaluation tree.
  ///
  struct DisplaySettings {
    /// Horizontal space between the drawing and the edge of the window, in
    /// characters.
    float PageBorderHorizontal;
    
    /// Vertical space between the drawing and the edge of the window, in
    /// characters.
    float PageBorderVertical;
    
    /// Space placed above a node's rectangle, in characters.
    float NodeBorderVertical;
    
    unsigned CodeFontSize;
    
    wxColour NodeBackground;
    
    wxColour NodeBorder;
    
    wxColour NodeHighlightedBackground;
    
    wxColour NodeHighlightedBorder;
    
    /// \brief Constructor.
    ///
    DisplaySettings();
  };
  
  /// Settings for the display of the evaluation tree.
  DisplaySettings Settings;
  
  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  /// The current process state.
  seec::cm::ProcessState const *CurrentProcess;
  
  /// The current thread state.
  seec::cm::ThreadState const *CurrentThread;
  
  /// The current active function.
  seec::cm::FunctionState const *ActiveFn;
  
  /// Font to use for drawing code and values.
  wxFont CodeFont;
  
  /// The pretty-printed Stmt.
  std::string Statement;
  
  /// Information for all sub-nodes in the Stmt.
  std::vector<NodeInfo> Nodes;
  
  /// The node that the mouse is currently over.
  decltype(Nodes)::const_iterator HoverNodeIt;

public:
  /// \brief Construct.
  ///
  StateEvaluationTreePanel();

  /// \brief Construct and create.
  ///
  StateEvaluationTreePanel(wxWindow *Parent,
                           ContextNotifier &WithNotifier,
                           wxWindowID ID = wxID_ANY,
                           wxPoint const &Position = wxDefaultPosition,
                           wxSize const &Size = wxDefaultSize);

  /// \brief Destructor.
  ///
  virtual ~StateEvaluationTreePanel();

  /// \brief Create (if default constructed).
  ///
  bool Create(wxWindow *Parent,
              ContextNotifier &WithNotifier,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Update this panel to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);

  /// \brief Clear the display of this panel.
  ///
  void clear();

  /// \brief Render this panel using the given \c wxDC.
  ///
  void render(wxDC &dc);

  /// \name Event Handling.
  /// @{
  
  void OnPaint(wxPaintEvent &);
  void OnMouseMoved(wxMouseEvent &);
  void OnMouseLeftWindow(wxMouseEvent &);
  
  /// @} (Event Handling)

public:
  DECLARE_EVENT_TABLE()
};

#endif // SEEC_TRACE_VIEW_STATEEVALUATIONTREE_HPP
