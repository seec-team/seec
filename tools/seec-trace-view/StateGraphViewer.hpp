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

#include <memory>
#include <mutex>


namespace seec {
  namespace cm {
    class ProcessState;
    class ThreadState;
    
    namespace graph {
      class LayoutHandler;
    }
  }
  
  class CallbackFSHandler;
}

class ContextNotifier;
class GraphRenderedEvent;
class StateAccessToken;
class wxWebView;
class wxWebViewEvent;


/// \brief Displays a collection of state viewers.
///
class StateGraphViewerPanel final : public wxPanel
{
  /// The central handler for context notifications.
  ContextNotifier *Notifier;
  
  /// The location of the dot executable.
  std::string PathToDot;
  
  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  /// The current process state.
  seec::cm::ProcessState const *CurrentProcess;
  
  /// The WebView used to display the rendered state graphs.
  wxWebView *WebView;
  
  /// The LayoutHandler used to generate dot state graphs.
  std::unique_ptr<seec::cm::graph::LayoutHandler> LayoutHandler;
  
  /// Control access to the LayoutHandler.
  std::mutex LayoutHandlerMutex;
  
  /// Virtual file system used to call functions from the WebView's javascript.
  seec::CallbackFSHandler *CallbackFS;

public:
  /// \brief Construct.
  ///
  StateGraphViewerPanel();

  /// \brief Construct and create.
  ///
  StateGraphViewerPanel(wxWindow *Parent,
                        ContextNotifier &WithNotifier,
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
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Handle a rendered graph.
  ///
  void OnGraphRendered(GraphRenderedEvent const &Ev);

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
};

#endif // SEEC_TRACE_VIEW_STATEGRAPHVIEWER_HPP
