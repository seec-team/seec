//===- tools/seec-trace-view/StateGraphViewer.cpp -------------------------===//
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

#include "seec/Clang/GraphLayout.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/CallbackFSHandler.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif

#if !wxUSE_WEBVIEW_WEBKIT && !wxUSE_WEBVIEW_IE
  #error "wxWebView backend required!"
#endif

#include <wx/webview.h>
#include <wx/webviewfshandler.h>
#include <wx/wfstream.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <gvc.h>

#include <memory>
#include <string>

#include "ProcessMoveEvent.hpp"
#include "StateGraphViewer.hpp"
#include "TraceViewerFrame.hpp"


//------------------------------------------------------------------------------
// GraphRenderedEvent
//------------------------------------------------------------------------------

/// \brief Used to send a rendered graph back to the GUI thread.
///
class GraphRenderedEvent : public wxEvent
{
  std::shared_ptr<wxString const> SetStateScript;
  
public:
  wxDECLARE_CLASS(GraphRenderedEvent);
  
  /// \brief Constructor.
  ///
  GraphRenderedEvent(wxEventType EventType,
                     int WinID,
                     std::shared_ptr<wxString const> WithSetStateScript)
  : wxEvent(WinID, EventType),
    SetStateScript(std::move(WithSetStateScript))
  {}
  
  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new GraphRenderedEvent(*this);
  }
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the SetState() script.
  ///
  wxString const &getSetStateScript() const { return *SetStateScript; }
  
  /// @} (Accessors.)
};

IMPLEMENT_CLASS(GraphRenderedEvent, wxEvent)

wxDEFINE_EVENT(SEEC_EV_GRAPH_RENDERED, GraphRenderedEvent);


//------------------------------------------------------------------------------
// StateGraphViewerPanel
//------------------------------------------------------------------------------

StateGraphViewerPanel::StateGraphViewerPanel()
: wxPanel(),
  CurrentAccess(),
  GraphvizContext(nullptr),
  WebView(nullptr),
  LayoutHandler(),
  LayoutHandlerMutex(),
  CallbackFS(nullptr)
{}

StateGraphViewerPanel::StateGraphViewerPanel(wxWindow *Parent,
                                             wxWindowID ID,
                                             wxPoint const &Position,
                                             wxSize const &Size)
: wxPanel(),
  CurrentAccess(),
  GraphvizContext(nullptr),
  WebView(nullptr),
  LayoutHandler(),
  LayoutHandlerMutex(),
  CallbackFS(nullptr)
{
  Create(Parent, ID, Position, Size);
}

StateGraphViewerPanel::~StateGraphViewerPanel()
{
  if (GraphvizContext)
    gvFreeContext(GraphvizContext);
  
  wxFileSystem::RemoveHandler(CallbackFS);
}

bool StateGraphViewerPanel::Create(wxWindow *Parent,
                                   wxWindowID ID,
                                   wxPoint const &Position,
                                   wxSize const &Size)
{
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;
  
  // Enable vfs access to request information about the state.
  auto const ThisAddr = reinterpret_cast<uintptr_t>(this);
  auto const CallbackProto = std::string{"seec"} + std::to_string(ThisAddr);
  
  CallbackFS = new seec::CallbackFSHandler(CallbackProto);
  
  // Value context menu callbacks.
  CallbackFS->addCallback("get_value_type",
    std::function<std::string (uintptr_t)>{
      [this] (uintptr_t const ValueID) -> std::string {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        return V.getTypeAsString();
      }
    });
  
  CallbackFS->addCallback("move_to_allocation",
    std::function<void (uintptr_t)>{
      [this] (uintptr_t const ValueID) -> void {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        
        raiseMovementEvent(*this, this->CurrentAccess,
          [&V] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveToAllocation(State, V);
          });
      }
    });

  CallbackFS->addCallback("move_to_deallocation",
    std::function<void (uintptr_t)>{
      [this] (uintptr_t const ValueID) -> void {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        
        raiseMovementEvent(*this, this->CurrentAccess,
          [&V] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveToDeallocation(State, V);
          });
      }
    });
  
  CallbackFS->addCallback("move_to_previous_state",
    std::function<void (uintptr_t)>{
      [this] (uintptr_t const ValueID) -> void {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        if (!V.isInMemory())
          return;
        
        auto const Size = V.getTypeSizeInChars().getQuantity();
        auto const Area = seec::MemoryArea(V.getAddress(), Size);
        
        raiseMovementEvent(*this, this->CurrentAccess,
          [Area] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveBackwardUntilMemoryChanges(State, Area);
          });
      }
    });
  
  CallbackFS->addCallback("move_to_next_state",
    std::function<void (uintptr_t)>{
      [this] (uintptr_t const ValueID) -> void {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        if (!V.isInMemory())
          return;
        
        auto const Size = V.getTypeSizeInChars().getQuantity();
        auto const Area = seec::MemoryArea(V.getAddress(), Size);
        
        raiseMovementEvent(*this, this->CurrentAccess,
          [Area] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveForwardUntilMemoryChanges(State, Area);
          });
      }
    });
  
  CallbackFS->addCallback("list_layout_engines_supporting_value",
    std::function<seec::callbackfs::Formatted<std::string> (uintptr_t)>{
      [this] (uintptr_t const ValueID)
        -> seec::callbackfs::Formatted<std::string>
      {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        bool First = true;
        
        std::string Result {'['};
        
        std::unique_lock<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
        auto const Engines = LayoutHandler->listLayoutEnginesSupporting(V);
        LockLayoutHandler.unlock();
        
        for (auto const E : Engines)
        {
          auto const LazyName = E->getName();
          if (!LazyName)
            continue;
          
          UErrorCode Status = U_ZERO_ERROR;
          auto const Name = LazyName->get(Status, Locale());
          if (U_FAILURE(Status))
            continue;
          
          if (First)
            First = false;
          else
            Result.push_back(',');
          
          Result.append("{id:");
          Result.append(std::to_string(reinterpret_cast<uintptr_t>(E)));
          Result.append(",name:\"");
          Name.toUTF8String(Result); // TODO: Escape this string.
          Result.append("\"}");
        }
        
        Result.push_back(']');
        
        return seec::callbackfs::Formatted<std::string>(std::move(Result));
      }
    });
  
  CallbackFS->addCallback("set_layout_engine_value",
    std::function<void (uintptr_t, uintptr_t)>{
      [this] (uintptr_t const EngineID, uintptr_t const ValueID) -> void {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        {
          std::lock_guard<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
          this->LayoutHandler->setLayoutEngine(V, EngineID);
        }
        this->renderGraph();
      }
    });
  
  // Function context menu callbacks.
  CallbackFS->addCallback("move_to_function_entry",
    std::function<void (uintptr_t)>{
      [this] (uintptr_t const FunctionID) -> void {
        auto &F =
          *reinterpret_cast<seec::cm::FunctionState *>(FunctionID);

        raiseMovementEvent(*this, this->CurrentAccess,
          [&F] (seec::cm::ProcessState &) -> bool {
            return seec::cm::moveToFunctionEntry(F);
          });
      }
    });

  CallbackFS->addCallback("move_to_function_finished",
    std::function<void (uintptr_t)>{
      [this] (uintptr_t const FunctionID) -> void {
        auto &F =
          *reinterpret_cast<seec::cm::FunctionState *>(FunctionID);

        raiseMovementEvent(*this, this->CurrentAccess,
          [&F] (seec::cm::ProcessState &) -> bool {
            return seec::cm::moveToFunctionFinished(F);
          });
      }
    });

  wxFileSystem::AddHandler(CallbackFS);
  
  // Get our resources from ICU.
  UErrorCode Status = U_ZERO_ERROR;
  auto Resources = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "StateGraphViewer");
  
  if (!U_SUCCESS(Status))
    return false;
  
  auto Sizer = new wxBoxSizer(wxVERTICAL);
  
  // Setup the webview.
  WebView = wxWebView::New(this, wxID_ANY);
  if (!WebView) {
    wxLogDebug("wxWebView::New failed.");
    return false;
  }
  
  WebView->RegisterHandler(wxSharedPtr<wxWebViewHandler>
                                      (new wxWebViewFSHandler("icurb")));
  WebView->RegisterHandler(wxSharedPtr<wxWebViewHandler>
                                      (new wxWebViewFSHandler(CallbackProto)));
  
  Sizer->Add(WebView, wxSizerFlags(1).Expand());
  SetSizerAndFit(Sizer);
  
  // Setup Graphviz.
  GraphvizContext = gvContext();
  
  // Setup the layout handler.
  {
    std::lock_guard<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
    LayoutHandler.reset(new seec::cm::graph::LayoutHandler());
    LayoutHandler->addBuiltinLayoutEngines();
  }
  
  // Load the webpage.
  auto const WebViewURL =
    std::string{"icurb://TraceViewer/StateGraphViewer/WebViewHTML#"}
    + CallbackProto;
  
  WebView->LoadURL(WebViewURL);
  
  // Wire up our event handlers.
  Bind(SEEC_EV_GRAPH_RENDERED, &StateGraphViewerPanel::OnGraphRendered, this);
  
  return true;
}

void StateGraphViewerPanel::OnGraphRendered(GraphRenderedEvent const &Ev)
{
  WebView->RunScript(Ev.getSetStateScript());
}

void StateGraphViewerPanel::renderGraph()
{
  if (!WebView)
    return;
  
  // Create a graph of the process state in dot format.
  std::string GraphString;
  
  {
    // Lock the current state while we read from it.
    auto Lock = CurrentAccess->getAccess();
    if (!Lock || !CurrentProcess)
      return;
      
    std::lock_guard<std::mutex> LockLayoutHandler (LayoutHandlerMutex);
    auto const Layout = LayoutHandler->doLayout(*CurrentProcess);
    GraphString = Layout.getDotString();
    
    auto const TimeMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                  (Layout.getTimeTaken());
    
    wxLogDebug("State graph generated in %" PRIu64 " ms.",
               static_cast<uint64_t>(TimeMS.count()));
  }
  
  auto const GVStart = std::chrono::steady_clock::now();
  
  std::unique_ptr<char []> Buffer {new char [GraphString.size() + 1]};
  if (!Buffer)
    return;
  
  memcpy(Buffer.get(), GraphString.data(), GraphString.size());
  Buffer[GraphString.size()] = 0;
  GraphString.clear();
  
  // Parse the graph into Graphviz's internal format.
  Agraph_t *Graph = agmemread(Buffer.get());
  
  auto const FreeGraph = seec::scopeExit([=] () { agclose(Graph); });
  
  // Layout the graph.
  gvLayout(GraphvizContext, Graph, "dot");
  
  auto const FreeLayout =
    seec::scopeExit([=] () { gvFreeLayout(GraphvizContext, Graph); });
  
  // Render the graph as SVG.
  char *RenderedData = nullptr;
  unsigned RenderedLength = 0;
  
  auto const FreeData =
    seec::scopeExit([=] () { if (RenderedData) free(RenderedData); });
  
  gvRenderData(GraphvizContext, Graph, "svg", &RenderedData, &RenderedLength);
  
  if (!RenderedData)
    return;
  
  auto const GVEnd = std::chrono::steady_clock::now();
  auto const GVMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                              (GVEnd - GVStart);
  wxLogDebug("Graphviz completed in %" PRIu64 " ms",
             static_cast<uint64_t>(GVMS.count()));
  
  // Remove all non-print characters from the SVG and send it to the WebView
  // via Javascript.
  auto SharedScript = std::make_shared<wxString>();
  auto &Script = *SharedScript;
  
  Script.reserve(RenderedLength + 256);
  Script << "SetState(\"";
  
  for (unsigned i = 0; i < RenderedLength; ++i) {
    if (std::isprint(RenderedData[i])) {
      if (RenderedData[i] == '\\' || RenderedData[i] == '"')
        Script << '\\';
      Script << RenderedData[i];
    }
  }
  
  Script << "\");";
  
  GraphRenderedEvent Ev {
    SEEC_EV_GRAPH_RENDERED,
    this->GetId(),
    std::move(SharedScript)
  };
  
  Ev.SetEventObject(this);
  
  this->GetEventHandler()->AddPendingEvent(Ev);
}

void
StateGraphViewerPanel::show(std::shared_ptr<StateAccessToken> Access,
                            seec::cm::ProcessState const &Process,
                            seec::cm::ThreadState const &Thread)
{
  CurrentAccess = std::move(Access);
  CurrentProcess = &Process;
  
  if (!WebView)
    return;
  
  renderGraph();
}

void StateGraphViewerPanel::clear()
{
  if (WebView)
    WebView->RunScript(wxString{"ClearState();"});
}
