//===- tools/seec-trace-view/ThreadStateViewer.hpp ------------------------===//
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

#ifndef SEEC_TRACE_VIEW_THREADSTATEVIEWER_HPP
#define SEEC_TRACE_VIEW_THREADSTATEVIEWER_HPP

#include <wx/wx.h>
#include <wx/scrolwin.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <vector>


#if 0 // NEEDS UPDATING TO USE seec::cm

class FunctionStateViewerPanel;
class OpenTrace;

namespace seec {
  namespace trace {
    class ThreadState;
  }
}


/// \brief Shows thread-specific state information for a single thread.
///
class ThreadStateViewerPanel : public wxScrolledWindow
{
  /// Holds the function state viewers.
  wxBoxSizer *Sizer;
  
  /// Panels for each active function.
  std::vector<FunctionStateViewerPanel *> FunctionViewers;
  
  /// The trace associated with this object.
  OpenTrace const *Trace;
  
public:
  /// \brief Construct without creating.
  ///
  ThreadStateViewerPanel()
  : wxScrolledWindow(),
    Sizer(nullptr),
    FunctionViewers()
  {}
  
  /// \brief Construct and create.
  ///
  ThreadStateViewerPanel(wxWindow *Parent,
                         OpenTrace const &TheTrace,
                         wxWindowID ID = wxID_ANY,
                         wxPoint const &Position = wxDefaultPosition,
                         wxSize const &Size = wxDefaultSize)
  : wxScrolledWindow(),
    Sizer(nullptr),
    FunctionViewers()
  {
    Create(Parent, TheTrace, ID, Position, Size);
  }
  
  /// \brief Destructor.
  ///
  virtual ~ThreadStateViewerPanel();
  
  /// \brief Create an object that was previously default-constructed.
  ///
  bool Create(wxWindow *Parent,
              OpenTrace const &TheTrace,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);
  
  /// \brief Update the display to show the given State.
  ///
  void showState(seec::trace::ThreadState const &State,
                 std::shared_ptr<seec::cm::ValueStore const> ValueStore);
};

#endif // NEEDS UPDATING TO USE seec::cm

#endif // SEEC_TRACE_VIEW_THREADSTATEVIEWER_HPP
