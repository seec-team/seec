//===- MallocViewer.cpp ---------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "MallocViewer.hpp"
#include "OpenTrace.hpp"

#include "seec/Trace/ProcessState.hpp"

MallocViewerPanel::~MallocViewerPanel() {
  //
}

bool MallocViewerPanel::Create(wxWindow *Parent,
                               wxWindowID ID,
                               wxPoint const &Position,
                               wxSize const &Size) {
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;

  return true;
}

void MallocViewerPanel::show(OpenTrace &TraceInfo,
                             seec::trace::ProcessState &State) {
  //
}
