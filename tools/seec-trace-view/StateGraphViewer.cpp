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
#include "seec/Util/ScopeExit.hpp"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif

#if !wxUSE_WEBVIEW_WEBKIT && !wxUSE_WEBVIEW_IE
  #error "wxWebView backend required!"
#endif

#include <wx/webview.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <gvc.h>

#include <memory>

#include "StateGraphViewer.hpp"
#include "TraceViewerFrame.hpp"


//------------------------------------------------------------------------------
// StateGraphViewerPanel
//------------------------------------------------------------------------------

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
  
  auto Sizer = new wxBoxSizer(wxVERTICAL);
  
  WebView = wxWebView::New(this, wxID_ANY);
  if (!WebView) {
    wxLogDebug("wxWebView::New failed.");
    return false;
  }
  
  Sizer->Add(WebView, wxSizerFlags(1).Expand());
  SetSizerAndFit(Sizer);
  
  GraphvizContext = gvContext();
  
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
    
    llvm::raw_string_ostream GraphStream {GraphString};
    seec::cm::writeDotGraph(Process, GraphStream);
  }
  
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
  
  // Show the SVG as the webpage.
  WebView->SetPage(wxString{RenderedData, RenderedLength}, wxString{});
}

void StateGraphViewerPanel::clear()
{
  WebView->SetPage(wxString{}, wxString{});
}
