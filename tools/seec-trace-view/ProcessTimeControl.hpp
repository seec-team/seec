//===- ProcessTimeControl.hpp ---------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_PROCESSTIMECONTROL_HPP
#define SEEC_TRACE_VIEW_PROCESSTIMECONTROL_HPP

#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"


// Forward-declarations.
class OpenTrace;


/// Represents events concerning the ProcessTime.
class ProcessTimeEvent : public wxEvent
{
  /// The ProcessTime associated with this event.
  uint64_t ProcessTime;

public:
  // Make this class known to wxWidgets' class hierarchy.
  wxDECLARE_CLASS(ProcessTimeEvent);

  /// Constructor.
  ProcessTimeEvent(wxEventType EventType, int WinID, uint64_t ProcessTime)
  : wxEvent(WinID, EventType),
    ProcessTime(ProcessTime)
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// Copy constructor.
  ProcessTimeEvent(ProcessTimeEvent const &Ev)
  : wxEvent(Ev),
    ProcessTime(Ev.ProcessTime)
  {
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// wxEvent::Clone().
  virtual wxEvent *Clone() const { return new ProcessTimeEvent(*this); }

  /// \name Accessors
  /// @{

  /// Get the ProcessTime associated with this event.
  uint64_t getProcessTime() const { return ProcessTime; }

  /// @}
};

// Produced when the user changes the process time.
wxDECLARE_EVENT(SEEC_EV_PROCESS_TIME_CHANGED, ProcessTimeEvent);

// Produced when the user is "viewing" a process time (e.g. mouse-over).
wxDECLARE_EVENT(SEEC_EV_PROCESS_TIME_VIEWED, ProcessTimeEvent);

/// Used inside an event table to catch SEEC_EV_PROCESS_TIME_CHANGED.
#define SEEC_EVT_PROCESS_TIME_CHANGED(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_PROCESS_TIME_CHANGED, id, (&func))

/// Used inside an event table to catch SEEC_EV_PROCESS_TIME_VIEWED.
#define SEEC_EVT_PROCESS_TIME_VIEWED(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_PROCESS_TIME_VIEWED, id, (&func))

/// A control that allows the user to navigate through the ProcessTime.
class ProcessTimeControl : public wxPanel
{
  /// Slider for the user to manipulate the process time.
  wxSlider *SlideProcessTime;

  /// Information about the currently open trace (if any).
  OpenTrace *Trace;

public:
  // Make this class known to wxWidgets' class hierarchy, and dynamically
  // creatable.
  DECLARE_DYNAMIC_CLASS(ProcessTimeControl)

  /// \brief Constructor (without creation).
  /// A ProcessTimeControl constructed with this constructor must later be
  /// created by calling Create().
  ProcessTimeControl()
  : wxPanel(),
    SlideProcessTime(nullptr),
    Trace(nullptr)
  {}

  /// \brief Constructor (with creation).
  ProcessTimeControl(wxWindow *Parent,
                     wxWindowID ID = wxID_ANY)
  : wxPanel(),
    SlideProcessTime(nullptr),
    Trace(nullptr)
  {
    Create(Parent, ID);
  }

  /// Create this object (if it was not created by the constructor).
  bool Create(wxWindow *Parent,
              wxWindowID ID = wxID_ANY);

  /// Set the currently open trace.
  void setTrace(OpenTrace &TraceData);

  /// Clear the currently open trace.
  void clearTrace();

  /// Called when the SlideProcessTime slider raises an event.
  void OnSlide(wxScrollEvent &Event);

private:
  // Declare the static event table (it is defined in ProcessTimeControl.cpp)
  DECLARE_EVENT_TABLE();
};


#endif // SEEC_TRACE_VIEW_PROCESSTIMECONTROL_HPP
