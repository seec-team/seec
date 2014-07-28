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
#include <wx/timer.h>
#include <wx/tipwin.h>
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

class ActionRecord;
class ActionReplayFrame;
class ContextEvent;
class ContextNotifier;
class StateAccessToken;


/// \brief Displays a collection of state viewers.
///
class StateEvaluationTreePanel final : public wxScrolled<wxPanel>
{
  /// \brief Types of decoration that may be applied to a node.
  ///
  enum class NodeDecoration {
    None,
    Active,
    Highlighted
  };
  
  /// \brief Information for a single node in the tree.
  ///
  struct NodeInfo {
    /// This node's Stmt.
    clang::Stmt const *Statement;
    
    /// Value produced by the evaluation of this node.
    std::shared_ptr<seec::cm::Value const> Value;
    
    /// String representation of the value produced by this node.
    wxString ValueString;
    
    /// Shortened string representation of the value produced by this node.
    wxString ValueStringShort;
    
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
             wxString WithValueString,
             wxString WithValueStringShort,
             uint64_t WithRangeStart,
             std::size_t WithRangeLength,
             unsigned WithDepth,
             wxCoord WithXStart,
             wxCoord WithXEnd,
             wxCoord WithYStart,
             wxCoord WithYEnd)
    : Statement(ForStatement),
      Value(std::move(WithValue)),
      ValueString(std::move(WithValueString)),
      ValueStringShort(std::move(WithValueStringShort)),
      RangeStart(WithRangeStart),
      RangeLength(WithRangeLength),
      Depth(WithDepth),
      XStart(WithXStart),
      XEnd(WithXEnd),
      YStart(WithYStart),
      YEnd(WithYEnd)
    {}
  };
  
  enum class IndicatorStyle {
    Plain,
    Box
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
    
    wxColour Background;

    wxColour Text;
    
    wxColour NodeBackground;
    
    wxColour NodeBorder;

    wxColour NodeText;
    
    wxColour NodeActiveBackground;
    
    wxColour NodeActiveBorder;

    wxColour NodeActiveText;
    
    wxColour NodeHighlightedBackground;
    
    wxColour NodeHighlightedBorder;

    wxColour NodeHighlightedText;
    
    /// \brief Constructor.
    ///
    DisplaySettings();
  };
  
  /// Settings for the display of the evaluation tree.
  DisplaySettings Settings;
  
  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// Used to record user interactions.
  ActionRecord *Recording;
  
  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  /// The current process state.
  seec::cm::ProcessState const *CurrentProcess;
  
  /// The current thread state.
  seec::cm::ThreadState const *CurrentThread;
  
  /// The current active function.
  seec::cm::FunctionState const *ActiveFn;
  
  /// Size required to draw the evaluation tree.
  wxSize CurrentSize;

  /// Font to use for drawing code and values.
  wxFont CodeFont;
  
  /// The pretty-printed Stmt.
  std::string Statement;
  
  /// Information for all sub-nodes in the Stmt.
  std::vector<NodeInfo> Nodes;
  
  /// The node that the mouse is currently over.
  decltype(Nodes)::const_iterator HoverNodeIt;
  
  /// Node that the user hovered over in the replay.
  decltype(Nodes)::const_iterator ReplayHoverNodeIt;
  
  /// Used to detect significant mouse hover over nodes.
  wxTimer HoverTimer;
  
  /// False if there was movement between mouse down and mouse up.
  bool ClickUnmoved;

  /// Highlighted Stmt.
  clang::Stmt const *HighlightedStmt;

  /// Highlighted Value.
  seec::cm::Value const *HighlightedValue;

  /// \brief Draw a single node using the given \c wxDC.
  ///
  void drawNode(wxDC &DC,
                NodeInfo const &Node,
                NodeDecoration const Decoration);

  /// \brief Render this panel using the given \c wxDC.
  ///
  void render(wxDC &dc);
  
  /// \brief Redraw this panel.
  ///
  void redraw();
  
  /// \brief Scroll the window to center on the given node.
  ///
  void centreOnNode(NodeInfo const &Node);
  
  /// \brief Set the node that the mouse is hovering over.
  /// \return true iff the hover node changed.
  ///
  bool setHoverNode(decltype(Nodes)::iterator It);
  
  /// \brief Show the hover tooltip for a node.
  ///
  void showHoverTooltip(NodeInfo const &Node);

  /// \brief Check if the tree contains a \c clang::Stmt.
  ///
  bool treeContainsStmt(clang::Stmt const *S) const;

  /// \brief Check if the tree contains a \c seec::cm::Value.
  ///
  bool treeContainsValue(seec::cm::Value const &V) const;

  /// \name Context events
  /// @{

  void notifyContextEvent(ContextEvent const &);

  /// @} (Context events)

  /// \name Replay events
  /// @{

  void ReplayNodeMouseOver(decltype(Nodes)::difference_type const NodeIndex,
                           clang::Stmt const *Stmt);
  void ReplayNodeRightClick(decltype(Nodes)::difference_type const NodeIndex,
                            clang::Stmt const *Stmt);
  void ReplayNodeHover(decltype(Nodes)::difference_type const NodeIndex,
                       clang::Stmt const *Stmt);

  /// @} (Replay events)

public:
  /// \brief Construct.
  ///
  StateEvaluationTreePanel();

  /// \brief Construct and create.
  ///
  StateEvaluationTreePanel(wxWindow *Parent,
                           ContextNotifier &WithNotifier,
                           ActionRecord &WithRecording,
                           ActionReplayFrame &WithReplay,
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
              ActionRecord &WithRecording,
              ActionReplayFrame &WithReplay,
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

  /// \name Event Handling.
  /// @{
  
  void OnPaint(wxPaintEvent &);
  void OnMouseMoved(wxMouseEvent &);
  void OnMouseLeftWindow(wxMouseEvent &);
  void OnMouseRightDown(wxMouseEvent &);
  void OnMouseRightUp(wxMouseEvent &);
  void OnHover(wxTimerEvent &);
  
  /// @} (Event Handling)

  /// \name Render to image.
  /// @{

  /// \brief Render the current dynamic evaluation tree to a bitmap.
  /// This will overwrite any existing file at the given location.
  /// \return true iff the bitmap was written successfully.
  ///
  bool renderToBMP(wxString const &Filename);

  /// @}

public:
  DECLARE_EVENT_TABLE()
};

#endif // SEEC_TRACE_VIEW_STATEEVALUATIONTREE_HPP
