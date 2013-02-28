//===- tools/seec-trace-view/StateViewer.cpp ------------------------------===//
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

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/dataview.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <set>
#include <map>
#include <memory>

#include "MallocViewer.hpp"
#include "StateViewer.hpp"
#include "OpenTrace.hpp"
#include "ThreadStateViewer.hpp"

//------------------------------------------------------------------------------
// StateViewerPanel
//------------------------------------------------------------------------------

StateViewerPanel::~StateViewerPanel() {}

bool StateViewerPanel::Create(wxWindow *Parent,
                              OpenTrace const &TheTrace,
                              wxWindowID ID,
                              wxPoint const &Position,
                              wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;
  
  // Set the associated trace.
  Trace = &TheTrace;

  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));
  
  // Setup thread-specific state information:
  auto NumThreads = Trace->getProcessTrace().getNumThreads();

  if (NumThreads == 1) {
    // Setup a single thread state viewer.
    auto ThreadViewer = new ThreadStateViewerPanel(this, *Trace);
    
    ThreadViewers.push_back(ThreadViewer);
  }
  else {
    // Setup a notebook with one page per thread.
    ThreadBook = new wxAuiNotebook(this,
                                   wxID_ANY,
                                   wxDefaultPosition,
                                   wxDefaultSize,
                                   wxAUI_NB_TOP
                                   | wxAUI_NB_TAB_SPLIT
                                   | wxAUI_NB_TAB_MOVE
                                   | wxAUI_NB_SCROLL_BUTTONS);
    
    auto ThreadNumberStr = TextTable.getStringEx("ThreadNumber", Status);
    
    // Add a page for each thread. ThreadIDs are 1-based.
    for (std::size_t i = 1; i <= NumThreads; ++i) {
      int64_t ThreadID = i;
      auto PageName = seec::format(ThreadNumberStr, Status, ThreadID);
      assert(U_SUCCESS(Status));
      
      auto ThreadViewer = new ThreadStateViewerPanel(this, *Trace);
      ThreadBook->AddPage(ThreadViewer, seec::towxString(PageName));
      
      ThreadViewers.push_back(ThreadViewer);
    }
  }

  
  // Setup the process-wide state information:
  
  // Create the notebook that holds other state views.
  StateBook = new wxAuiNotebook(this,
                                wxID_ANY,
                                wxDefaultPosition,
                                wxDefaultSize,
                                wxAUI_NB_TOP
                                | wxAUI_NB_TAB_SPLIT
                                | wxAUI_NB_TAB_MOVE
                                | wxAUI_NB_SCROLL_BUTTONS);

  // Create the MallocViewer and add it to the notebook.
  MallocViewer = new MallocViewerPanel(this);
  StateBook->AddPage(MallocViewer,
                     seec::getwxStringExOrEmpty(TextTable, "MallocView_Title"));


  // Use a sizer to layout the thread view and process view notebook.
  auto TopSizer = new wxGridSizer(1, // Rows
                                  2, // Cols
                                  wxSize(0,0) // Gap
                                  );
  
  if (ThreadBook)
    TopSizer->Add(ThreadBook, wxSizerFlags().Expand());
  else
    TopSizer->Add(ThreadViewers[0], wxSizerFlags().Expand());
  
  TopSizer->Add(StateBook, wxSizerFlags().Expand());
  
  SetSizerAndFit(TopSizer);

  return true;
}

void
StateViewerPanel::show(seec::trace::ProcessState &State,
                       std::shared_ptr<seec::cm::ValueStore const> ValueStore)
{
  // Update thread-specific views.
  auto &Threads = State.getThreadStates();
  
  for (std::size_t i = 0; i < Threads.size(); ++i) {
    ThreadViewers[i]->showState(*Threads[i], ValueStore);
  }
  
  // Update the malloc state view.
  MallocViewer->show(State);
}

void StateViewerPanel::clear() {
  // Clear thread-specific views.
  
  // Clear the malloc state view.
  MallocViewer->clear();
}
