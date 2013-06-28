//===- tools/seec-trace-view/ThreadMoveEvent.hpp --------------------------===//
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

#ifndef SEEC_TRACE_VIEW_THREADMOVEEVENT_HPP
#define SEEC_TRACE_VIEW_THREADMOVEEVENT_HPP

#include "seec/Clang/MappedThreadState.hpp"

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <functional>
#include <memory>

class StateAccessToken;

/// \brief Represents events requesting thread movement.
///
class ThreadMoveEvent : public wxEvent
{
  /// The thread associated with this event.
  size_t ThreadIndex;
  
  /// Callback that will move the event.
  std::function<bool (seec::cm::ThreadState &State)> Mover;

public:
  // Make this class known to wxWidgets' class hierarchy.
  wxDECLARE_CLASS(ThreadMoveEvent);

  /// \brief Constructor.
  ///
  ThreadMoveEvent(wxEventType EventType,
                  int WinID,
                  size_t ForThreadIndex,
                  std::function<bool (seec::cm::ThreadState &State)> WithMover)
  : wxEvent(WinID, EventType),
    ThreadIndex(ForThreadIndex),
    Mover(std::move(WithMover))
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// \brief Copy constructor.
  ///
  ThreadMoveEvent(ThreadMoveEvent const &Ev)
  : wxEvent(Ev),
    ThreadIndex(Ev.ThreadIndex),
    Mover(Ev.Mover)
  {
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new ThreadMoveEvent(*this);
  }

  /// \name Accessors
  /// @{
  
  size_t getThreadIndex() const { return ThreadIndex; }
  
  decltype(Mover) const &getMover() const { return Mover; }
  
  /// @}
};

// Produced when the user changes the thread time.
wxDECLARE_EVENT(SEEC_EV_THREAD_MOVE, ThreadMoveEvent);

/// Used inside an event table to catch SEEC_EV_THREAD_MOVE.
#define SEEC_EVT_THREAD_MOVE(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_THREAD_MOVE, id, (&func))


void
raiseMovementEvent(wxWindow &Control,
                   std::shared_ptr<StateAccessToken> &Access,
                   std::size_t const ThreadIndex,
                   std::function<bool (seec::cm::ThreadState &State)> Mover);

#endif // SEEC_TRACE_VIEW_THREADMOVEEVENT_HPP
