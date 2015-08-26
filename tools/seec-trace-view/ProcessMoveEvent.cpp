//===- tools/seec-trace-view/ProcessMoveEvent.cpp -------------------------===//
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

#include "ProcessMoveEvent.hpp"
#include "StateAccessToken.hpp"

IMPLEMENT_CLASS(ProcessMoveEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_PROCESS_MOVE, ProcessMoveEvent);

void
raiseMovementEvent(
  wxWindow &Control,
  std::shared_ptr<StateAccessToken> &Access,
  std::function<seec::cm::MovementResult (seec::cm::ProcessState &State)> Mover)
{
  auto const Handler = Control.GetEventHandler();
  if (!Handler) {
    wxLogDebug("raiseMovementEvent: wxWindow does not have an event handler.");
    return;
  }
  
  if (!Access) {
    wxLogDebug("raiseMovementEvent: no access provided.");
    return;
  }
  
  auto LockAccess = Access->getAccess();
  if (!LockAccess) { // Token is out of date.
    wxLogDebug("raiseMovementEvent: access token is outdated.");
    return;
  }
  
  ProcessMoveEvent Ev {
    SEEC_EV_PROCESS_MOVE,
    Control.GetId(),
    std::move(Mover)
  };
  
  Ev.SetEventObject(&Control);
  
  LockAccess.release();
  
  Handler->AddPendingEvent(Ev);
}
