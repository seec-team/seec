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
class ActionRecord;
class ActionReplayFrame;
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
  ActionRecord *Recording;
  
  wxButton *ButtonGoToStart;
  
  wxButton *ButtonStepBackTopLevel;

  wxButton *ButtonStepBack;
  
  wxButton *ButtonStepForward;
  
  wxButton *ButtonStepForwardTopLevel;

  // wxButton *ButtonGoToNextError;
  
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
    Recording(nullptr),
    ButtonGoToStart(nullptr),
    ButtonStepBackTopLevel(nullptr),
    ButtonStepBack(nullptr),
    ButtonStepForward(nullptr),
    ButtonStepForwardTopLevel(nullptr),
    // ButtonGoToNextError(nullptr),
    ButtonGoToEnd(nullptr),
    CurrentAccess(),
    CurrentThreadIndex()
  {}

  /// \brief Constructor (with creation).
  ///
  ThreadTimeControl(wxWindow *Parent,
                    ActionRecord &WithRecord,
                    ActionReplayFrame *WithReplay)
  : ThreadTimeControl()
  {
    Create(Parent, WithRecord, WithReplay);
  }

  /// \brief Create this object (if it was not created by the constructor).
  ///
  bool Create(wxWindow *Parent,
              ActionRecord &WithRecord,
              ActionReplayFrame *WithReplay);
  
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
  
private:
  void GoToStart();
  void StepBackTopLevel();
  void StepBack();
  void StepForward();
  void StepForwardTopLevel();
  void GoToNextError();
  void GoToEnd();
};


#endif // SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP
