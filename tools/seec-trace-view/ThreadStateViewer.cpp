//===- tools/seec-trace-view/ThreadStateViewer.cpp ------------------------===//
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

#include "seec/Trace/ThreadState.hpp"

#include "FunctionStateViewer.hpp"
#include "ThreadStateViewer.hpp"


//------------------------------------------------------------------------------
// ThreadStateViewerPanel
//------------------------------------------------------------------------------

ThreadStateViewerPanel::~ThreadStateViewerPanel() {}

bool ThreadStateViewerPanel::Create(wxWindow *Parent,
                                    OpenTrace const &TheTrace,
                                    wxWindowID ID,
                                    wxPoint const &Position,
                                    wxSize const &Size) {
  if (!wxScrolledWindow::Create(Parent, ID, Position, Size))
    return false;

  Sizer = new wxBoxSizer(wxVERTICAL);
  SetSizer(Sizer);
  
  Trace = &TheTrace;
  
  return true;
}

void
ThreadStateViewerPanel::
showState(seec::trace::ThreadState const &State,
          std::shared_ptr<seec::cm::ValueStore const> ValueStore)
{
  // Destroy all existing function viewers.
  for (auto FunctionViewer : FunctionViewers) {
    Sizer->Detach(FunctionViewer);
    FunctionViewer->Destroy();
  }
  
  FunctionViewers.clear();
  
  // Add fresh new function viewers.
  auto &CallStack = State.getCallStack();
  
  for (auto &State : CallStack) {
    auto Viewer = new FunctionStateViewerPanel(this,
                                               *Trace,
                                               *State,
                                               ValueStore);
    
    FunctionViewers.push_back(Viewer);
    
    Sizer->Add(Viewer, wxSizerFlags().Proportion(0).Expand());
  }
  
  Layout();
}
