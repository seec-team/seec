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

#include "seec/Clang/DotGraph.hpp"
#include "seec/Clang/GraphLayout.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/ScopeExit.hpp"
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
  }
  
  wxLogDebug("Graph in dot:\n%s", GraphString);
  
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
  
  // Remove all non-print characters from the SVG.
  std::string SVGString(RenderedData, RenderedLength);
  
  // TODO: Trim everything before <svg
  
  for (std::string::size_type i = 0; i < SVGString.length(); ) {
    if (!std::isprint(SVGString[i])) {
      SVGString.erase(i, 1);
      continue;
    }
    
    if (SVGString[i] == '\\' || SVGString[i] == '"') {
      SVGString.insert(i, 1, '\\');
      i += 2;
      continue;
    }
    
    ++i;
  }
  
  // Send the SVG to the webpage via javascript.
  wxString Script;
  Script << "SetState(\"" << SVGString << "\");";
  
  wxLogDebug("Setting state to:\n%s", SVGString);
  
  WebView->RunScript(Script);
}

void StateGraphViewerPanel::clear()
{
  if (WebView)
    WebView->RunScript(wxString{"ClearState();"});
}
