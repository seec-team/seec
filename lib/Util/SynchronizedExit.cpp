//===- Util/SynchronizedExit.cpp ------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>

namespace seec {

void SynchronizedExit::stop(std::unique_lock<std::mutex> &Lock) {
  // Add this thread to the stop.
  if (++StoppedThreads >= NumThreads)
    AllThreadsStopped.notify_one();
    
  // Sleep and release Lock.
  StopCancelled.wait(Lock);
}

void SynchronizedExit::threadStart() {
  std::unique_lock<std::mutex> Lock(Access);
  
  if (ExitCalled)
    return;
    
  ++NumThreads;
    
  if (StoppedThreads)
    stop(Lock);
}

void SynchronizedExit::threadFinish() {
  std::unique_lock<std::mutex> Lock(Access);
  
  if (ExitCalled)
    return;
    
  if (StoppedThreads)
    stop(Lock);
      
  --NumThreads;
}

void SynchronizedExit::exit(int ExitCode) {
  // Scope to control the lifetime of the lock on Access
  {
    std::unique_lock<std::mutex> Lock(Access);
  
    // Exit already started by another thread, so we will join that.
    if (StoppedThreads) {
      stop(Lock);
    }

    // Wait until all threads have synchronized for the exit.
    if (++StoppedThreads != NumThreads) {
      AllThreadsStopped.wait(Lock, [&](){return NumThreads == StoppedThreads;});
    }
    
    ExitCalled = true;
  }
  
  std::exit(ExitCode);
}

}
