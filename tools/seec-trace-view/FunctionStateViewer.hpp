//===- FunctionStateViewer.hpp --------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_FUNCTIONSTATEVIEWER_HPP
#define SEEC_TRACE_VIEW_FUNCTIONSTATEVIEWER_HPP

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

class OpenTrace;

namespace seec {
  namespace trace {
    class FunctionState;
  }
}


/// \brief Shows state information for a single function invocation.
///
class FunctionStateViewerPanel : public wxPanel
{
  /// The trace associated with this object.
  OpenTrace const *Trace;
  
  ///
  wxStaticText *Title;
  
public:
  FunctionStateViewerPanel()
  : wxPanel(),
    Trace(nullptr),
    Title(nullptr)
  {}
  
  FunctionStateViewerPanel(wxWindow *Parent,
                           OpenTrace const &TheTrace,
                           wxWindowID ID = wxID_ANY,
                           wxPoint const &Position = wxDefaultPosition,
                           wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    Trace(nullptr),
    Title(nullptr)
  {
    Create(Parent, TheTrace, ID, Position, Size);
  }
  
  virtual ~FunctionStateViewerPanel();
  
  bool Create(wxWindow *Parent,
              OpenTrace const &TheTrace,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  /// \brief Update the display to show the given State.
  ///
  void showState(seec::trace::FunctionState const &State);
};

#endif // SEEC_TRACE_VIEW_FUNCTIONSTATEVIEWER_HPP
