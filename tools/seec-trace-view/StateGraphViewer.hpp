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

#include <gvc.h>

#include <memory>


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

class StateAccessToken;
class wxWebView;
class wxWebViewEvent;


/// \brief Displays a collection of state viewers.
///
class StateGraphViewerPanel final : public wxPanel
{
  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  GVC_t *GraphvizContext;
  
  wxWebView *WebView;
  
  std::unique_ptr<seec::cm::graph::LayoutHandler> LayoutHandler;
  
  seec::CallbackFSHandler *CallbackFS;

public:
  StateGraphViewerPanel();

  StateGraphViewerPanel(wxWindow *Parent,
                        wxWindowID ID = wxID_ANY,
                        wxPoint const &Position = wxDefaultPosition,
                        wxSize const &Size = wxDefaultSize);

  ~StateGraphViewerPanel();

  bool Create(wxWindow *Parent,
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
};

#endif // SEEC_TRACE_VIEW_STATEGRAPHVIEWER_HPP
