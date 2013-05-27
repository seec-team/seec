//===- tools/seec-trace-view/StateViewer.hpp ------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_STATEVIEWER_HPP
#define SEEC_TRACE_VIEW_STATEVIEWER_HPP

#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>


namespace seec {
  namespace cm {
    class ProcessState;
    class ThreadState;
  }
}

class StateAccessToken;
class wxAuiNotebook;


/// \brief Displays a collection of state viewers.
///
class StateViewerPanel final : public wxPanel
{
  /// Holds all state viewer panels.
  wxAuiNotebook *StateBook;
  
  // MallocViewerPanel *MallocViewer;
  
  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;

public:
  StateViewerPanel()
  : wxPanel(),
    StateBook(nullptr)
  {}

  StateViewerPanel(wxWindow *Parent,
                   wxWindowID ID = wxID_ANY,
                   wxPoint const &Position = wxDefaultPosition,
                   wxSize const &Size = wxDefaultSize)
  : wxPanel(),
    StateBook(nullptr)
  {
    Create(Parent, ID, Position, Size);
  }

  ~StateViewerPanel();

  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Update this panel to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);

  void clear();
};

#endif // SEEC_TRACE_VIEW_STATEVIEWER_HPP
