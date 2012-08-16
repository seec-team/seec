//===- ThreadTimeControl.hpp ----------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP
#define SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP

#include <wx/wx.h>
#include <wx/panel.h>
#include "seec/wxWidgets/CleanPreprocessor.h"


// Forward-declarations.
class OpenTrace;

namespace seec {
  namespace trace {
    class ThreadTrace;
  } // namespace trace (in seec)
} // namespace seec


/// \brief Represents events concerning the ThreadTime of a thread.
///
class ThreadTimeEvent : public wxEvent
{
  /// The Thread ID of the thread associated with this event.
  uint32_t ThreadID;

  /// The thread trace associated with this event.
  uint64_t ThreadTime;

public:
  // Make this class known to wxWidgets' class hierarchy.
  wxDECLARE_CLASS(ThreadTimeEvent);

  /// Constructor.
  ThreadTimeEvent(wxEventType EventType,
                  int WinID,
                  uint32_t ThreadID,
                  uint64_t ThreadTime)
  : wxEvent(WinID, EventType),
    ThreadID(ThreadID),
    ThreadTime(ThreadTime)
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// Copy constructor.
  ThreadTimeEvent(ThreadTimeEvent const &Ev)
  : wxEvent(Ev),
    ThreadID(Ev.ThreadID),
    ThreadTime(Ev.ThreadTime)
  {
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// wxEvent::Clone().
  virtual wxEvent *Clone() const { return new ThreadTimeEvent(*this); }

  /// \name Accessors
  /// @{

  /// Get the Thread ID of the thread associated with this event.
  uint32_t getThreadID() const { return ThreadID; }

  /// Get the ThreadTime associated with this event.
  uint64_t getThreadTime() const { return ThreadTime; }

  /// @}
};

// Produced when the user changes the thread time.
wxDECLARE_EVENT(SEEC_EV_THREAD_TIME_CHANGED, ThreadTimeEvent);

// Produced when the user is "viewing" a thread time (e.g. mouse-over).
wxDECLARE_EVENT(SEEC_EV_THREAD_TIME_VIEWED, ThreadTimeEvent);

/// Used inside an event table to catch SEEC_EV_THREAD_TIME_CHANGED.
#define SEEC_EVT_THREAD_TIME_CHANGED(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_THREAD_TIME_CHANGED, id, (&func))

/// Used inside an event table to catch SEEC_EV_THREAD_TIME_VIEWED.
#define SEEC_EVT_PROCESS_TIME_VIEWED(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_PROCESS_TIME_VIEWED, id, (&func))


/// \brief A control that allows the user to navigate through the ThreadTime.
///
class ThreadTimeControl : public wxPanel
{
  /// Slider for the user to manipulate the thread time.
  wxSlider *SlideThreadTime;

  /// Information about the currently open trace (if any).
  OpenTrace *Trace;

  /// Trace for the thread that we're controlling.
  seec::trace::ThreadTrace const *ThreadTrace;

public:
  // Make this class known to wxWidgets' class hierarchy, and dynamically
  // creatable.
  DECLARE_DYNAMIC_CLASS(ThreadTimeControl)

  /// \brief Constructor (without creation).
  /// A ThreadTimeControl constructed with this constructor must later be
  /// created by calling Create().
  ThreadTimeControl()
  : wxPanel(),
    SlideThreadTime(nullptr),
    Trace(nullptr),
    ThreadTrace(nullptr)
  {}

  /// \brief Constructor (with creation).
  ThreadTimeControl(wxWindow *Parent,
                    OpenTrace &TheTrace,
                    seec::trace::ThreadTrace const &TheThreadTrace,
                    wxWindowID ID = wxID_ANY)
  : wxPanel(),
    SlideThreadTime(nullptr),
    Trace(nullptr),
    ThreadTrace(nullptr)
  {
    Create(Parent, TheTrace, TheThreadTrace, ID);
  }

  /// Create this object (if it was not created by the constructor).
  bool Create(wxWindow *Parent,
              OpenTrace &TheTrace,
              seec::trace::ThreadTrace const &TheThreadTrace,
              wxWindowID ID = wxID_ANY);

  /// Called when the SlideThreadTime slider raises an event.
  void OnSlide(wxScrollEvent &Event);

private:
  // Declare the static event table (it is defined in ThreadTimeControl.cpp)
  DECLARE_EVENT_TABLE();
};


#endif // SEEC_TRACE_VIEW_THREADTIMECONTROL_HPP