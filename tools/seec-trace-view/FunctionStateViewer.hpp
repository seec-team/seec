//===- tools/seec-trace-view/FunctionStateViewer.hpp ----------------------===//
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

#ifndef SEEC_TRACE_VIEW_FUNCTIONSTATEVIEWER_HPP
#define SEEC_TRACE_VIEW_FUNCTIONSTATEVIEWER_HPP

#include <wx/wx.h>

#include <memory>

#if 0 // NEEDS UPDATING TO USE seec::cm

class OpenTrace;


/// \brief Shows state information for a single function invocation.
///
class FunctionStateViewerPanel : public wxPanel
{
  /// The trace associated with this object.
  OpenTrace const *Trace;
  
  ///
  wxStaticBoxSizer *Container;
  
public:
  FunctionStateViewerPanel()
  : wxPanel(),
    Trace(nullptr),
    Container(nullptr)
  {}
  
  FunctionStateViewerPanel(
    wxWindow *Parent,
    OpenTrace const &TheTrace,
    seec::trace::FunctionState const &State,
    std::shared_ptr<seec::cm::ValueStore const> ValueStore,
    wxWindowID ID = wxID_ANY,
    wxPoint const &Position = wxDefaultPosition,
    wxSize const &Size = wxDefaultSize
  )
  : wxPanel(),
    Trace(nullptr),
    Container(nullptr)
  {
    Create(Parent, TheTrace, State, ValueStore, ID, Position, Size);
  }
  
  virtual ~FunctionStateViewerPanel();
  
  bool Create(wxWindow *Parent,
              OpenTrace const &TheTrace,
              seec::trace::FunctionState const &State,
              std::shared_ptr<seec::cm::ValueStore const> ValueStore,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
};

#endif // NEEDS UPDATING TO USE seec::cm

#endif // SEEC_TRACE_VIEW_FUNCTIONSTATEVIEWER_HPP
