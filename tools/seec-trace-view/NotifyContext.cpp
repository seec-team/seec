//===- tools/seec-trace-view/NotifyContext.cpp ----------------------------===//
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

#include "NotifyContext.hpp"


//===----------------------------------------------------------------------===//
// ContextNotifier
//===----------------------------------------------------------------------===//

ContextNotifier::CallbackIterTy
ContextNotifier::callbackAdd(CallbackTy Callback)
{
  std::lock_guard<std::mutex> Lock (CallbacksMutex);
  
  return Callbacks.insert(Callbacks.end(), std::move(Callback));
}

void ContextNotifier::callbackRemove(CallbackIterTy It)
{
  std::lock_guard<std::mutex> Lock (CallbacksMutex);
  
  Callbacks.erase(It);
}

ContextNotifier::ListenerIterTy
ContextNotifier::listenerAdd(ContextListener *Listener)
{
  std::lock_guard<std::mutex> Lock (ListenersMutex);
  
  return Listeners.insert(Listeners.end(), Listener);
}

void ContextNotifier::listenerRemove(ListenerIterTy It)
{
  std::lock_guard<std::mutex> Lock (ListenersMutex);
  
  Listeners.erase(It);
}

void ContextNotifier::notify(ContextEvent const &Ev) const
{
  std::lock_guard<std::mutex> LockCallbacks (CallbacksMutex);
  std::lock_guard<std::mutex> LockListeners (ListenersMutex);
  
  for (auto &Callback : Callbacks) {
    Callback(Ev);
  }
  
  for (auto const Listener : Listeners) {
    Listener->notifyContextEvent(Ev);
  }
}
