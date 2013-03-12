//===- lib/Runtimes/Tracer/WrapCstdlib_h.cpp ------------------------------===//
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

#include "Tracer.hpp"

#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Support/CallSite.h"

#include <cstdlib>
#include <stack>


namespace seec {


/// \brief Stop all other threads and write trace information.
///
/// This prepares us to safely terminate the process.
///
void stopThreadsAndWriteTrace() {
  auto &ProcessEnv = seec::trace::getProcessEnvironment();
  auto &ProcessListener = ProcessEnv.getProcessListener();
  
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &ThreadListener = ThreadEnv.getThreadListener();
  
  // Interact with the thread listener's notification system.
  ThreadListener.enterNotification();
  
  // Stop all of the other threads.
  auto const &SupportSyncExit = ThreadListener.getSupportSynchronizedExit();
  SupportSyncExit.getSynchronizedExit().stopAll();
  
  // TODO: Write an event for this Instruction.
  
  // Write out the trace information (if tracing is enabled).
  auto const TraceEnabled = ProcessListener.traceEnabled();
  
  if (TraceEnabled) {
    ProcessListener.traceWrite();
    ProcessListener.traceFlush();
    ProcessListener.traceClose();
    
    for (auto const ThreadListenerPtr : ProcessListener.getThreadListeners()) {
      ThreadListenerPtr->traceWrite();
      ThreadListenerPtr->traceFlush();
      ThreadListenerPtr->traceClose();
    }
  }
}


static std::stack<void (*)()> AtExitFunctions;

static std::mutex AtExitFunctionsMutex;

static std::stack<void (*)()> AtQuickExitFunctions;

static std::mutex AtQuickExitFunctionsMutex;


} // namespace seec


extern "C" {


//===----------------------------------------------------------------------===//
// abort
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(abort)
()
{
  seec::stopThreadsAndWriteTrace();
  std::abort();
}


//===----------------------------------------------------------------------===//
// exit
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(exit)
(int exit_code)
{
  // Call intercepted atexit() registered functions.
  {
    std::lock_guard<std::mutex> Lock (seec::AtExitFunctionsMutex);
    
    while (!seec::AtExitFunctions.empty()) {
      auto Fn = seec::AtExitFunctions.top();
      seec::AtExitFunctions.pop();
      (*Fn)();
    }
  }
  
  seec::stopThreadsAndWriteTrace();
  std::exit(exit_code);
}


//===----------------------------------------------------------------------===//
// quick_exit
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(quick_exit)
(int exit_code)
{
  // Call intercepted at_quick_exit() registered functions.
  {
    std::lock_guard<std::mutex> Lock (seec::AtQuickExitFunctionsMutex);
    
    while (!seec::AtQuickExitFunctions.empty()) {
      auto Fn = seec::AtQuickExitFunctions.top();
      seec::AtQuickExitFunctions.pop();
      (*Fn)();
    }
  }
  
  seec::stopThreadsAndWriteTrace();
  std::_Exit(exit_code);
}


//===----------------------------------------------------------------------===//
// _Exit
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(_Exit)
(int exit_code)
{
  seec::stopThreadsAndWriteTrace();
  std::_Exit(exit_code);
}


//===----------------------------------------------------------------------===//
// atexit
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(atexit)
(void (*func)())
{
  std::lock_guard<std::mutex> Lock (seec::AtExitFunctionsMutex);
  
  seec::AtExitFunctions.push(func);
  
  return 0;
}


//===----------------------------------------------------------------------===//
// at_quick_exit
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(at_quick_exit)
(void (*func)())
{
  std::lock_guard<std::mutex> Lock (seec::AtQuickExitFunctionsMutex);
  
  seec::AtQuickExitFunctions.push(func);
  
  return 0;
}


} // extern "C"
