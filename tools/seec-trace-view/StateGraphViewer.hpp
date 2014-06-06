//===- tools/seec-trace-view/StateGraphViewer.hpp -------------------------===//
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

#ifndef SEEC_TRACE_VIEW_STATEGRAPHVIEWER_HPP
#define SEEC_TRACE_VIEW_STATEGRAPHVIEWER_HPP

#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>


namespace seec {
  namespace cm {
    class FunctionState;
    class LocalState;
    class ParamState;
    class ProcessState;
    class ThreadState;
    class Value;
    class ValueOfPointer;
    
    namespace graph {
      class LayoutHandler;
    }
  }
  
  class CallbackFSHandler;
}

class ActionRecord;
class ActionReplayFrame;
class ContextNotifier;
class GraphRenderedEvent;
class MouseOverDisplayableEvent;
class StateAccessToken;
class wxWebView;
class wxWebViewEvent;


/// \brief Something that a user might see and interact with.
///
class Displayable
{
public:
  enum class Kind {
    Value,
    Dereference,
    FunctionState,
    LocalState,
    ParamState,
    ReferencedArea
  };
  
private:
  Kind const ThisKind;
  
public:
  Displayable(Kind WithKind)
  : ThisKind(WithKind)
  {}
  
  Kind getKind() const { return ThisKind; }
  
  virtual ~Displayable();
};


/// \brief A displayed Value that the user may interact with.
///
class DisplayableValue final : public Displayable
{
  seec::cm::Value const &TheValue;
  
public:
  DisplayableValue(seec::cm::Value const &ForValue)
  : Displayable(Displayable::Kind::Value),
    TheValue(ForValue)
  {}
  
  seec::cm::Value const &getValue() const { return TheValue; }
  
  static bool classof(Displayable const *D) {
    return D && D->getKind() == Displayable::Kind::Value;
  }
};


/// \brief A displayed dereference that the user may interact with.
///
class DisplayableDereference final : public Displayable
{
  seec::cm::ValueOfPointer const &ThePointer;
  
public:
  DisplayableDereference(seec::cm::ValueOfPointer const &OfPointer)
  : Displayable(Displayable::Kind::Dereference),
    ThePointer(OfPointer)
  {}
  
  seec::cm::ValueOfPointer const &getPointer() const { return ThePointer; }
  
  static bool classof(Displayable const *D) {
    return D && D->getKind() == Displayable::Kind::Dereference;
  }
};


/// \brief A displayed FunctionState that the user may interact with.
///
class DisplayableFunctionState final : public Displayable
{
  seec::cm::FunctionState &TheFunctionState;
  
public:
  DisplayableFunctionState(seec::cm::FunctionState &ForFunctionState)
  : Displayable(Displayable::Kind::FunctionState),
    TheFunctionState(ForFunctionState)
  {}
  
  seec::cm::FunctionState &getFunctionState() const {
    return TheFunctionState;
  }
  
  static bool classof(Displayable const *D) {
    return D && D->getKind() == Displayable::Kind::FunctionState;
  }
};


/// \brief A displayed LocalState that the user may interact with.
///
class DisplayableLocalState final : public Displayable
{
  seec::cm::LocalState const &TheLocalState;

public:
  DisplayableLocalState(seec::cm::LocalState const &ForLocalState)
  : Displayable(Displayable::Kind::LocalState),
    TheLocalState(ForLocalState)
  {}

  seec::cm::LocalState const &getLocalState() const {
    return TheLocalState;
  }

  static bool classof(Displayable const *D) {
    return D && D->getKind() == Displayable::Kind::LocalState;
  }
};


/// \brief A displayed ParamState that the user may interact with.
///
class DisplayableParamState final : public Displayable
{
  seec::cm::ParamState const &TheParamState;

public:
  DisplayableParamState(seec::cm::ParamState const &ForParamState)
  : Displayable(Displayable::Kind::ParamState),
    TheParamState(ForParamState)
  {}

  seec::cm::ParamState const &getParamState() const {
    return TheParamState;
  }

  static bool classof(Displayable const *D) {
    return D && D->getKind() == Displayable::Kind::ParamState;
  }
};


/// \brief A displayed area interpreted from a given pointer.
///
class DisplayableReferencedArea final : public Displayable
{
  uint64_t AreaStart;
  
  uint64_t AreaEnd;
  
  seec::cm::ValueOfPointer const &ThePointer;
  
public:
  DisplayableReferencedArea(uint64_t const WithAreaStart,
                            uint64_t const WithAreaEnd,
                            seec::cm::ValueOfPointer const &OfPointer)
  : Displayable(Displayable::Kind::ReferencedArea),
    AreaStart(WithAreaStart),
    AreaEnd(WithAreaEnd),
    ThePointer(OfPointer)
  {}
  
  uint64_t getAreaStart() const { return AreaStart; }
  
  uint64_t getAreaEnd() const { return AreaEnd; }
  
  seec::cm::ValueOfPointer const &getPointer() const { return ThePointer; }
  
  static bool classof(Displayable const *D) {
    return D && D->getKind() == Displayable::Kind::ReferencedArea;
  }
};


/// \brief Displays a collection of state viewers.
///
class StateGraphViewerPanel final : public wxPanel
{
  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// Used to record user interactions.
  ActionRecord *Recording;

  /// The location of the dot executable.
  std::string PathToDot;
  
  /// The location of the graphviz libraries.
  std::string PathToGraphvizLibraries;
  
  /// The location of the graphviz plugins.
  std::string PathToGraphvizPlugins;

  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  /// The current process state.
  seec::cm::ProcessState const *CurrentProcess;

  /// A worker thread that generates the graphs.
  std::thread WorkerThread;

  /// Controls access to the worker's information.
  std::mutex TaskMutex;

  /// Used to notify the worker that a new task is available.
  std::condition_variable TaskCV;

  /// Access token for the worker's process state.
  std::shared_ptr<StateAccessToken> TaskAccess;

  /// Worker's process state.
  seec::cm::ProcessState const *TaskProcess;

  /// The WebView used to display the rendered state graphs.
  wxWebView *WebView;
  
  /// The LayoutHandler used to generate dot state graphs.
  std::unique_ptr<seec::cm::graph::LayoutHandler> LayoutHandler;
  
  /// Control access to the LayoutHandler.
  std::mutex LayoutHandlerMutex;
  
  /// Virtual file system used to call functions from the WebView's javascript.
  seec::CallbackFSHandler *CallbackFS;
  
  /// What the user's mouse is currently over.
  std::shared_ptr<Displayable const> MouseOver;

  /// \brief Generate the dot graph for \c TaskProcess.
  ///
  std::string workerGenerateDot();

  /// \brief Implements the worker thread's task loop.
  ///
  void workerTaskLoop();

  /// \brief Handle contextual events.
  ///
  void handleContextEvent(ContextEvent const &Ev);

public:
  /// \brief Construct.
  ///
  StateGraphViewerPanel();

  /// \brief Construct and create.
  ///
  StateGraphViewerPanel(wxWindow *Parent,
                        ContextNotifier &WithNotifier,
                        ActionRecord &WithRecording,
                        ActionReplayFrame &WithReplay,
                        wxWindowID ID = wxID_ANY,
                        wxPoint const &Position = wxDefaultPosition,
                        wxSize const &Size = wxDefaultSize);

  /// \brief Destructor.
  ///
  ~StateGraphViewerPanel();

  /// \brief Create (if default constructed).
  ///
  bool Create(wxWindow *Parent,
              ContextNotifier &WithNotifier,
              ActionRecord &WithRecording,
              ActionReplayFrame &WithReplay,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Handle a rendered graph.
  ///
  void OnGraphRendered(GraphRenderedEvent const &Ev);
  
  /// \brief Handle mouse over a Displayable.
  ///
  void OnMouseOverDisplayable(MouseOverDisplayableEvent const &Ev);
  
  /// \brief Generate a context menu.
  ///
  void OnContextMenu(wxContextMenuEvent &Ev);

  /// \brief Render a graph for the current process state.
  ///
  void renderGraph();

  /// \brief Update this panel to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);

  /// \brief Clear the display of this panel.
  ///
  void clear();

private:
  /// \brief Notify that the mouse is over a node.
  ///
  void OnMouseOver(std::string const &NodeID);
  
  /// \brief Notification from WebView that we should create a context menu.
  ///
  void RaiseContextMenu();
};

#endif // SEEC_TRACE_VIEW_STATEGRAPHVIEWER_HPP
