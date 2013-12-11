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
  struct NodeInfo {
    clang::Stmt const *Statement;
    
    std::shared_ptr<seec::cm::Value const> Value;
    
    uint64_t RangeStart;
    
    std::size_t RangeLength;
    
    unsigned Depth;
    
    wxCoord XStart;
    
    wxCoord XEnd;
    
    wxCoord YPos;
    
    NodeInfo(clang::Stmt const *ForStatement,
             std::shared_ptr<seec::cm::Value const> WithValue,
             uint64_t WithRangeStart,
             std::size_t WithRangeLength,
             unsigned WithDepth,
             wxCoord WithXStart,
             wxCoord WithXEnd,
             wxCoord WithYPos)
    : Statement(ForStatement),
      Value(WithValue),
      RangeStart(WithRangeStart),
      RangeLength(WithRangeLength),
      Depth(WithDepth),
      XStart(WithXStart),
      XEnd(WithXEnd),
      YPos(WithYPos)
    {}
  };
  
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

private:
  void render(wxDC &dc);

  void OnPaint(wxPaintEvent &);

public:
  DECLARE_EVENT_TABLE()
};

#endif // SEEC_TRACE_VIEW_STATEEVALUATIONTREE_HPP
