//===- lib/Util/SynchronizedExit.cpp --------------------------------------===//
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

#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>

namespace seec {

bool SynchronizedExit::initiateStop(std::unique_lock<std::mutex> &Lock) {
  // Stop already started by another thread, so we will join that.
  if (StoppedThreads) {
    return false;
  }

  assert(StopMaster == std::thread::id());
  StopMaster = std::this_thread::get_id();

  // Wait until all threads have synchronized for the exit.
  if (++StoppedThreads < NumThreads) {
    AllThreadsStopped.wait(Lock, [&](){ return StoppedThreads >= NumThreads; });
  }

  return true;
}

void SynchronizedExit::joinStop(std::unique_lock<std::mutex> &Lock) {
  // If this thread is the stop master then don't join the stop (this occurs
  // when the stop master terminates the program and thread listeners are
  // being destroyed).
  if (StopMaster == std::this_thread::get_id())
    return;
  
  // Add this thread to the stop.
  if (++StoppedThreads >= NumThreads)
    AllThreadsStopped.notify_one();
    
  // Sleep and release Lock.
  StopCancelled.wait(Lock);
  
  // Remove this thread from the stop.
  --StoppedThreads;
}

void SynchronizedExit::cancelStop() {
  AllThreadsStopped.notify_all();
}

void SynchronizedExit::threadStart() {
  std::unique_lock<std::mutex> Lock(Access);
  
  ++NumThreads;
  
  if (StoppedThreads)
    joinStop(Lock);
}

void SynchronizedExit::threadFinish() {
  std::unique_lock<std::mutex> Lock(Access);
  
  // We allow threads to finish if the exit() has already been called, because
  // the stopped threads may be calling threadFinish() during destruction.
  if (ExitCalled)
    return;
  
  if (StoppedThreads)
    joinStop(Lock);
  
  --NumThreads;
}

bool SynchronizedExit::StopCanceller::cancelStop() {
  if (!Stopped)
    return false;

  if (Restarted)
    return false;

  SE.cancelStop();
  return true;
}

SynchronizedExit::StopCanceller SynchronizedExit::stopAll() {
  std::unique_lock<std::mutex> Lock(Access);
  auto const Stopped = initiateStop(Lock);
  return StopCanceller(*this, Stopped);
}

void SynchronizedExit::abort() {
  {
    std::unique_lock<std::mutex> Lock(Access);
    while (!initiateStop(Lock))
      joinStop(Lock);
    ExitCalled = true;
  }
  std::abort();
}

void SynchronizedExit::exit(int ExitCode) {
  {
    std::unique_lock<std::mutex> Lock(Access);
    while (!initiateStop(Lock))
      joinStop(Lock);
    ExitCalled = true;
  }
  std::exit(ExitCode);
}

}
