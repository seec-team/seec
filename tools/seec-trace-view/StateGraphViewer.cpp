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
#include "seec/ICU/Resources.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/ICUBundleFSHandler.hpp"
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
  LayoutHandler()
{}

StateGraphViewerPanel::StateGraphViewerPanel(wxWindow *Parent,
                                             wxWindowID ID,
                                             wxPoint const &Position,
                                             wxSize const &Size)
: wxPanel(),
  CurrentAccess(),
  GraphvizContext(nullptr),
  WebView(nullptr),
  LayoutHandler()
{
  Create(Parent, ID, Position, Size);
}

StateGraphViewerPanel::~StateGraphViewerPanel()
{
  if (GraphvizContext)
    gvFreeContext(GraphvizContext);
}

bool StateGraphViewerPanel::Create(wxWindow *Parent,
                                   wxWindowID ID,
                                   wxPoint const &Position,
                                   wxSize const &Size)
{
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;
  
  // Enable wxWidgets virtual file system access to the ICU bundles.
  wxFileSystem::AddHandler(new seec::ICUBundleFSHandler());
  
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
  
  Sizer->Add(WebView, wxSizerFlags(1).Expand());
  SetSizerAndFit(Sizer);
  
  // Setup Graphviz.
  GraphvizContext = gvContext();
  
  // Setup the layout handler.
  LayoutHandler.reset(new seec::cm::graph::LayoutHandler());
  LayoutHandler->addBuiltinLayoutEngines();
  
  // Load the webpage.
  auto const HTMLResource = Resources.get("WebViewHTML", Status);
  if (!U_SUCCESS(Status)) {
    wxLogDebug("Couldn't get WebViewHTML!");
    return false;
  }
  
  int32_t BinLength = 0;
  auto const BinData = HTMLResource.getBinary(BinLength, Status);
  if (!U_SUCCESS(Status)) {
    wxLogDebug("Couldn't get binary!");
    return false;
  }
  
  wxString HTMLString {reinterpret_cast<char const *>(BinData),
                       std::size_t(BinLength)};
  
  WebView->SetPage(HTMLString, wxString{});
  
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
    
    uint64_t const TimeTakenNS = Layout.getTimeTaken().count();
    wxLogDebug("State graph generated in %" PRIu64 " ns.", TimeTakenNS);
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
  auto const GVNS = std::chrono::duration_cast<std::chrono::nanoseconds>
                                              (GVEnd - GVStart);
  wxLogDebug("Graphviz completed in %" PRIu64 " ns", static_cast<uint64_t>
                                                                (GVNS.count()));
  
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
  auto const GVFixNS = std::chrono::duration_cast<std::chrono::nanoseconds>
                                                 (GVFixEnd - GVEnd);
  wxLogDebug("String fixed in %" PRIu64 " ns", static_cast<uint64_t>
                                                          (GVFixNS.count()));
  
  WebView->RunScript(Script);
  
  auto const ScriptEnd = std::chrono::steady_clock::now();
  auto const ScriptNS = std::chrono::duration_cast<std::chrono::nanoseconds>
                                                  (ScriptEnd - GVFixEnd);
  wxLogDebug("Script run in %" PRIu64 " ns", static_cast<uint64_t>
                                                        (ScriptNS.count()));
}

void StateGraphViewerPanel::clear()
{
  if (WebView)
    WebView->RunScript(wxString{"ClearState();"});
}
