//===- tools/seec-trace-view/ThreadMoveEvent.cpp --------------------------===//
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

#include "StateAccessToken.hpp"
#include "ThreadMoveEvent.hpp"

IMPLEMENT_CLASS(ThreadMoveEvent, wxEvent)
wxDEFINE_EVENT(SEEC_EV_THREAD_MOVE, ThreadMoveEvent);

void
raiseMovementEvent(wxWindow &Control,
                   std::shared_ptr<StateAccessToken> &Access,
                   std::size_t const ThreadIndex,
                   std::function<bool (seec::cm::ThreadState &State)> Mover)
{
  auto const Handler = Control.GetEventHandler();
  if (!Handler)
    return;
  
  if (!Access)
    return;
  
  auto Lock = Access->getAccess();
  if (!Lock) // Token is out of date.
    return;
  
  ThreadMoveEvent Ev {
    SEEC_EV_THREAD_MOVE,
    Control.GetId(),
    ThreadIndex,
    std::move(Mover)
  };
  
  Ev.SetEventObject(&Control);
  
  Lock.unlock();
  
  Handler->AddPendingEvent(Ev);
}
