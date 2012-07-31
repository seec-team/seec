//===- MallocViewer.hpp ---------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_MALLOCVIEWER_HPP
#define SEEC_TRACE_VIEW_MALLOCVIEWER_HPP

#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

namespace seec {
namespace trace {
class ProcessState;
}
}

class OpenTrace;
class MallocListModel;
class wxDataViewCtrl;

///
class MallocViewerPanel : public wxPanel
{
  MallocListModel *DataModel;

  wxDataViewCtrl *DataView;

public:
  MallocViewerPanel()
  : DataModel(nullptr),
    DataView(nullptr)
  {}

  MallocViewerPanel(wxWindow *Parent,
                    wxWindowID ID = wxID_ANY,
                    wxPoint const &Position = wxDefaultPosition,
                    wxSize const &Size = wxDefaultSize)
  : DataModel(nullptr),
    DataView(nullptr)
  {
    Create(Parent, ID, Position, Size);
  }

  ~MallocViewerPanel();

  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  void show(OpenTrace &TraceInfo, seec::trace::ProcessState &State);

  void clear();
};

#endif // SEEC_TRACE_VIEW_MALLOCVIEWER_HPP
