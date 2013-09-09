//===- lib/Runtimes/Tracer/WrapPOSIXsys_wait_h ----------------------------===//
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

#include <type_traits>

#include <sys/wait.h>


extern "C" {


//===----------------------------------------------------------------------===//
// wait
//===----------------------------------------------------------------------===//

pid_t
SEEC_MANGLE_FUNCTION(wait)
(int *stat_loc)
{
  using namespace seec::trace;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Instruction = ThreadEnv.getInstruction();
  auto InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Ensure that writing to *stat_loc will be OK.
  if (stat_loc) {
    // Lock global memory.
    Listener.acquireGlobalMemoryWriteLock();
    
    // Use a CIOChecker to help check memory.
    auto FSFunction = seec::runtime_errors::format_selects::CStdFunction::wait;
    CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};
    
    Checker.checkMemoryExistsAndAccessibleForParameter
              (0,
               reinterpret_cast<uintptr_t>(stat_loc),
               sizeof(*stat_loc),
               seec::runtime_errors::format_selects::MemoryAccess::Write);
  }
  
  auto Result = wait(stat_loc);
  
  // Record the result.
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<pid_t>::type(Result));
  
  // Record the write to *stat_loc.
  if (stat_loc)
    Listener.recordUntypedState(reinterpret_cast<char const *>(stat_loc),
                                sizeof(*stat_loc));
  
  return Result;
}


} // extern "C"
