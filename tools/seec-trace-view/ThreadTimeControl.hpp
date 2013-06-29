//===- tools/seec-trace-view/ThreadTimeControl.hpp ------------------------===//
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

#ifndef SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP
#define SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP


#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <functional>
#include <memory>


// Forward-declarations.
class StateAccessToken;
class wxButton;

namespace seec {
  namespace cm {
    class ProcessState;
    class ThreadState;
  }
}


/// \brief A control that allows the user to navigate through the ThreadTime.
///
class ThreadTimeControl : public wxPanel
{
  wxButton *ButtonGoToStart;
  
  wxButton *ButtonStepBack;
  
  wxButton *ButtonStepForward;
  
  wxButton *ButtonGoToNextError;
  
  wxButton *ButtonGoToEnd;
  
  std::shared_ptr<StateAccessToken> CurrentAccess;
  
  size_t CurrentThreadIndex;
  
  /// \brief Disable all controls.
  ///
  void disableAll();
  
public:
  // Make this class known to wxWidgets' class hierarchy, and dynamically
  // creatable.
  DECLARE_DYNAMIC_CLASS(ThreadTimeControl)

  /// \brief Constructor (without creation).
  /// A ThreadTimeControl constructed with this constructor must later be
  /// created by calling Create().
  ///
  ThreadTimeControl()
  : wxPanel(),
    ButtonGoToStart(nullptr),
    ButtonStepBack(nullptr),
    ButtonStepForward(nullptr),
    ButtonGoToNextError(nullptr),
    ButtonGoToEnd(nullptr),
    CurrentAccess(),
    CurrentThreadIndex()
  {}

  /// \brief Constructor (with creation).
  ///
  ThreadTimeControl(wxWindow *Parent,
                    wxWindowID ID = wxID_ANY)
  : wxPanel(),
    ButtonGoToStart(nullptr),
    ButtonStepBack(nullptr),
    ButtonStepForward(nullptr),
    ButtonGoToNextError(nullptr),
    ButtonGoToEnd(nullptr),
    CurrentAccess(),
    CurrentThreadIndex()
  {
    Create(Parent, ID);
  }

  /// \brief Create this object (if it was not created by the constructor).
  ///
  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY);
  
  /// \brief Destructor.
  ///
  virtual ~ThreadTimeControl();

  /// \brief Update this control to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread,
            size_t ThreadIndex);

  /// \name Event Handlers
  /// @{

  /// \brief Called when the GoToStart button is clicked.
  ///
  void OnGoToStart(wxCommandEvent &Event);

  /// \brief Called when the StepBack button is clicked.
  ///
  void OnStepBack(wxCommandEvent &Event);

  /// \brief Called when the StepForward button is clicked.
  ///
  void OnStepForward(wxCommandEvent &Event);

  /// \brief Called when the GoToNextError button is clicked.
  ///
  void OnGoToNextError(wxCommandEvent &Event);

  /// \brief Called when the GoToEnd button is clicked.
  ///
  void OnGoToEnd(wxCommandEvent &Event);

  /// @} (Event Handlers)

private:
  // Declare the static event table (it is defined in ThreadTimeControl.cpp)
  DECLARE_EVENT_TABLE();
};


#endif // SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP
