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
raiseMovementEvent(wxWindow &Control,
                   std::shared_ptr<StateAccessToken> &Access,
                   std::function<bool (seec::cm::ProcessState &State)> Mover)
{
  auto const Handler = Control.GetEventHandler();
  if (!Handler)
    return;
  
  if (!Access)
    return;
  
  auto Lock = Access->getAccess();
  if (!Lock) // Token is out of date.
    return;
  
  ProcessMoveEvent Ev {
    SEEC_EV_PROCESS_MOVE,
    Control.GetId(),
    std::move(Mover)
  };
  
  Ev.SetEventObject(&Control);
  
  Lock.unlock();
  
  Handler->AddPendingEvent(Ev);
}
