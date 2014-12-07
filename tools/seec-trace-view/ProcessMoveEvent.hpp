//===- tools/seec-trace-view/ProcessMoveEvent.hpp -------------------------===//
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

#ifndef SEEC_TRACE_VIEW_PROCESSMOVEEVENT_HPP
#define SEEC_TRACE_VIEW_PROCESSMOVEEVENT_HPP

#include "seec/Clang/MappedProcessState.hpp"

#include <wx/wx.h>

#include <functional>
#include <memory>

class StateAccessToken;

namespace seec {
  namespace cm {
    enum class MovementResult;
  }
}

/// \brief Represents events requesting process state movement.
///
class ProcessMoveEvent : public wxEvent
{
  typedef
    std::function<seec::cm::MovementResult (seec::cm::ProcessState &State)>
    MoverTy;
  
  /// Callback that will move the state.
  MoverTy Mover;

public:
  // Make this class known to wxWidgets' class hierarchy.
  wxDECLARE_CLASS(ProcessMoveEvent);

  /// \brief Constructor.
  ///
  ProcessMoveEvent(wxEventType EventType, int WinID, MoverTy WithMover)
  : wxEvent(WinID, EventType),
    Mover(std::move(WithMover))
  {
    this->m_propagationLevel = wxEVENT_PROPAGATE_MAX;
  }

  /// \brief Copy constructor.
  ///
  ProcessMoveEvent(ProcessMoveEvent const &Ev)
  : wxEvent(Ev),
    Mover(Ev.Mover)
  {
    this->m_propagationLevel = Ev.m_propagationLevel;
  }

  /// \brief wxEvent::Clone().
  ///
  virtual wxEvent *Clone() const {
    return new ProcessMoveEvent(*this);
  }

  /// \name Accessors
  /// @{
  
  decltype(Mover) const &getMover() const { return Mover; }
  
  /// @}
};

// Produced when the user changes the thread time.
wxDECLARE_EVENT(SEEC_EV_PROCESS_MOVE, ProcessMoveEvent);

/// Used inside an event table to catch SEEC_EV_PROCESS_MOVE.
#define SEEC_EVT_PROCESS_MOVE(id, func) \
  wx__DECLARE_EVT1(SEEC_EV_PROCESS_MOVE, id, (&func))


void
raiseMovementEvent(
  wxWindow &Control,
  std::shared_ptr<StateAccessToken> &Access,
  std::function<seec::cm::MovementResult (seec::cm::ProcessState &State)> Mover
);

#endif // SEEC_TRACE_VIEW_PROCESSMOVEEVENT_HPP
