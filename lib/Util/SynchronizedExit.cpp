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

void SynchronizedExit::initiateStop() {
  std::unique_lock<std::mutex> Lock(Access);

  // Stop already started by another thread, so we will join that.
  if (StoppedThreads) {
    joinStop(Lock);
    return;
  }

  // Wait until all threads have synchronized for the exit.
  if (++StoppedThreads < NumThreads) {
    AllThreadsStopped.wait(Lock, [&](){ return StoppedThreads >= NumThreads; });
  }
  
  ExitCalled = true;
}

void SynchronizedExit::joinStop(std::unique_lock<std::mutex> &Lock) {
  // Add this thread to the stop.
  if (++StoppedThreads >= NumThreads)
    AllThreadsStopped.notify_one();
    
  // Sleep and release Lock.
  StopCancelled.wait(Lock);
  
  // Remove this thread from the stop.
  --StoppedThreads;
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

void SynchronizedExit::stopAll() {
  initiateStop();
}

void SynchronizedExit::abort() {
  initiateStop();
  std::abort();
}

void SynchronizedExit::exit(int ExitCode) {
  initiateStop();  
  std::exit(ExitCode);
}

}
