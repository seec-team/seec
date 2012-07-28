//===- StateViewer.hpp ----------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_STATEVIEWER_HPP
#define SEEC_TRACE_VIEW_STATEVIEWER_HPP

#include "seec/Trace/ProcessState.hpp"

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/panel.h>

class OpenTrace;
class StateTreeModel;

///
class StateViewerPanel : public wxPanel
{
  StateTreeModel *StateTree;
  
  wxDataViewCtrl *DataViewCtrl;
  
public:
  StateViewerPanel(wxWindow *Parent,
                   wxWindowID ID = wxID_ANY,
                   wxPoint const &Position = wxDefaultPosition,
                   wxSize const &Size = wxDefaultSize);
  
  ~StateViewerPanel();
  
  void show(OpenTrace &TraceInfo, seec::trace::ProcessState &State);
};

#endif // SEEC_TRACE_VIEW_STATEVIEWER_HPP
