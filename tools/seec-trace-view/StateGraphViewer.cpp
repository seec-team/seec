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
#include "seec/Clang/MappedValue.hpp"
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

#include "StateGraphViewer.hpp"
#include "TraceViewerFrame.hpp"


//------------------------------------------------------------------------------
// StateGraphViewerPanel
//------------------------------------------------------------------------------

StateGraphViewerPanel::StateGraphViewerPanel()
: wxPanel(),
  CurrentAccess(),
  GraphvizContext(nullptr),
  WebView(nullptr),
  LayoutHandler(),
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
  
  CallbackFS->addCallback("get_value_type",
    std::function<std::string (uintptr_t)>{
      [this] (uintptr_t ValueID) -> std::string {
        auto const &V = *reinterpret_cast<seec::cm::Value const *>(ValueID);
        return V.getTypeAsString();
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
  LayoutHandler.reset(new seec::cm::graph::LayoutHandler());
  LayoutHandler->addBuiltinLayoutEngines();
  
  // Load the webpage.
  auto const WebViewURL =
    std::string{"icurb:TraceViewer/StateGraphViewer/WebViewHTML#"}
    + CallbackProto;
  
  WebView->LoadURL(WebViewURL);
  
  return true;
}

void
StateGraphViewerPanel::show(std::shared_ptr<StateAccessToken> Access,
                            seec::cm::ProcessState const &Process,
                            seec::cm::ThreadState const &Thread)
{
  CurrentAccess = std::move(Access);
  
  if (!WebView)
    return;
  
  // Create a graph of the process state in dot format.
  std::string GraphString;
  
  {
    // Lock the current state while we read from it.
    auto Lock = CurrentAccess->getAccess();
    if (!Lock)
      return;
    
    // llvm::raw_string_ostream GraphStream {GraphString};
    // seec::cm::writeDotGraph(Process, GraphStream);
    
    auto const Layout = LayoutHandler->doLayout(Process);
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
  
  auto const GVEnd = std::chrono::steady_clock::now();
  auto const GVMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                              (GVEnd - GVStart);
  wxLogDebug("Graphviz completed in %" PRIu64 " ms", static_cast<uint64_t>
                                                                (GVMS.count()));
  
  // Remove all non-print characters from the SVG and send it to the WebView
  // via Javascript.
  wxString Script;
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
  
  auto const GVFixEnd = std::chrono::steady_clock::now();
  auto const GVFixMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                 (GVFixEnd - GVEnd);
  wxLogDebug("String fixed in %" PRIu64 " ms", static_cast<uint64_t>
                                                          (GVFixMS.count()));
  
  WebView->RunScript(Script);
  
  auto const ScriptEnd = std::chrono::steady_clock::now();
  auto const ScriptMS = std::chrono::duration_cast<std::chrono::milliseconds>
                                                  (ScriptEnd - GVFixEnd);
  wxLogDebug("Script run in %" PRIu64 " ms", static_cast<uint64_t>
                                                        (ScriptMS.count()));
}

void StateGraphViewerPanel::clear()
{
  if (WebView)
    WebView->RunScript(wxString{"ClearState();"});
}
