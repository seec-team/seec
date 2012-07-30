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
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

class OpenTrace;
class StateTreeModel;
class wxAuiNotebook;
class wxDataViewCtrl;

///
class StateViewerPanel : public wxPanel
{
  StateTreeModel *StateTree;

  wxDataViewCtrl *DataViewCtrl;

  wxAuiNotebook *StateBook;

public:
  StateViewerPanel()
  : wxPanel(),
    StateTree(nullptr),
    DataViewCtrl(nullptr),
    StateBook(nullptr)
  {}

  StateViewerPanel(wxWindow *Parent,
                   wxWindowID ID = wxID_ANY,
                   wxPoint const &Position = wxDefaultPosition,
                   wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    StateTree(nullptr),
    DataViewCtrl(nullptr),
    StateBook(nullptr)
  {
    Create(Parent, ID, Position, Size);
  }

  ~StateViewerPanel();

  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  void show(OpenTrace &TraceInfo, seec::trace::ProcessState &State);
};

#endif // SEEC_TRACE_VIEW_STATEVIEWER_HPP
