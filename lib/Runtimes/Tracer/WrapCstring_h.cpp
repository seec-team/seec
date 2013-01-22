//===- lib/Runtimes/Tracer/WrapCstring_h.cpp ------------------------------===//
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

#include <cstring>


extern "C" {

//===----------------------------------------------------------------------===//
// strdup
//===----------------------------------------------------------------------===//

char *
SEEC_MANGLE_FUNCTION(strdup)
(char const *String)
{
  using namespace seec::trace;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Instruction = ThreadEnv.getInstruction();
  auto InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Lock global memory.
  Listener.acquireGlobalMemoryWriteLock();
  Listener.acquireDynamicMemoryLock();
  
  // Use a CIOChecker to help check memory.
  auto FSFunction = seec::runtime_errors::format_selects::CStdFunction::strdup;
  CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};
  
  // Ensure that String is accessible.
  Checker.checkCStringRead(0, String);
  
  auto Result = strdup(String);
  
  // Record the result.
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       Result);
  
  if (Result) {
    auto Size = std::strlen(Result) + 1;
    Listener.recordMalloc(reinterpret_cast<uintptr_t>(Result), Size);
    Listener.recordUntypedState(Result, Size);
  }
  else{
    Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                                sizeof(errno));
  }
  
  return Result;
}


} // extern "C"
