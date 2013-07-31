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

#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>

#include "StateGraphViewer.hpp"
#include "StateViewer.hpp"


//------------------------------------------------------------------------------
// StateViewerPanel
//------------------------------------------------------------------------------

StateViewerPanel::~StateViewerPanel() = default;

bool StateViewerPanel::Create(wxWindow *Parent,
                              ContextNotifier &WithNotifier,
                              wxWindowID ID,
                              wxPoint const &Position,
                              wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;
  
  // Get the GUIText from the TraceViewer ICU resources.
  UErrorCode Status = U_ZERO_ERROR;
  auto TextTable = seec::getResource("TraceViewer",
                                     Locale::getDefault(),
                                     Status,
                                     "GUIText");
  assert(U_SUCCESS(Status));
  
  // Create the notebook that holds other state views.
  StateBook = new wxAuiNotebook(this,
                                wxID_ANY,
                                wxDefaultPosition,
                                wxDefaultSize,
                                wxAUI_NB_TOP
                                | wxAUI_NB_TAB_SPLIT
                                | wxAUI_NB_TAB_MOVE
                                | wxAUI_NB_SCROLL_BUTTONS);

#if 0 // TODO.
  // Create the MallocViewer and add it to the notebook.
  MallocViewer = new MallocViewerPanel(this);
  StateBook->AddPage(MallocViewer,
                     seec::getwxStringExOrEmpty(TextTable, "MallocView_Title"));

#endif
  
  // Create the graph viewer.
  GraphViewer = new StateGraphViewerPanel(this, WithNotifier);
  StateBook->AddPage(GraphViewer, wxString("Graph"));

  // Use a sizer to layout the thread view and process view notebook.
  auto TopSizer = new wxGridSizer(/* Rows */ 1,
                                  /* Cols */ 1,
                                  /* Gap */  wxSize(0,0));
  
  TopSizer->Add(StateBook, wxSizerFlags().Expand());
  
  SetSizerAndFit(TopSizer);

  return true;
}

void
StateViewerPanel::show(std::shared_ptr<StateAccessToken> Access,
                       seec::cm::ProcessState const &Process,
                       seec::cm::ThreadState const &Thread)
{
  CurrentAccess = std::move(Access);
  
  if (GraphViewer)
    GraphViewer->show(CurrentAccess, Process, Thread);
}

void StateViewerPanel::clear()
{
  CurrentAccess.reset();
  
  if (GraphViewer)
    GraphViewer->clear();
}
