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
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/dataview.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <set>
#include <map>
#include <memory>

#include "StateViewer.hpp"
#include "OpenTrace.hpp"


//------------------------------------------------------------------------------
// StateViewerPanel
//------------------------------------------------------------------------------

StateViewerPanel::~StateViewerPanel() = default;

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
  // TODO: Forward to individual viewers.
}

void StateViewerPanel::clear() {
  // TODO: Forward to individual viewers.
}
