//===- lib/Runtimes/Tracer/WrapPOSIXunistd_h.cpp --------------------------===//
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

#include <cinttypes>
#include <type_traits>

#include <unistd.h>


extern "C" {


//===----------------------------------------------------------------------===//
// execve
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execve)
(char const *filename, char * const argv[], char * const envp[])
{
  exit(EXIT_SUCCESS);
  return 0;
}


//===----------------------------------------------------------------------===//
// fork
//===----------------------------------------------------------------------===//

pid_t
SEEC_MANGLE_FUNCTION(fork)
()
{
  using namespace seec::trace;
  
  // Wait until all other threads are synchronized, waiting for our signal.
  
  auto Result = fork();
  
  if (Result == 0) {
    // This is the child process. We need to modify our tracing environment so
    // that we don't interfere with the parent process. Any other threads that
    // are waiting for us will need to update any environment references that
    // they are currently using (alternatively, no other threads should be
    // allowed to have an environment reference at the synchronization point).
    _exit(0);
  }
  
  // All other threads can continue execution now.
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyValue(ThreadEnv.getInstructionIndex(),
                       ThreadEnv.getInstruction(),
                       std::make_unsigned<pid_t>::type(Result));
  
  return Result;
}


//===----------------------------------------------------------------------===//
// fork
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(pipe)
(int pipefd[2])
{
  using namespace seec::trace;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Instruction = ThreadEnv.getInstruction();
  auto InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Lock global memory and streams.
  Listener.acquireGlobalMemoryWriteLock();
  
  // Use a CIOChecker to help check memory.
  auto FSFunction = seec::runtime_errors::format_selects::CStdFunction::pipe;
  CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};
  
  Checker.checkMemoryExistsAndAccessibleForParameter
            (0,
             reinterpret_cast<uintptr_t>(pipefd),
             sizeof(int [2]),
             seec::runtime_errors::format_selects::MemoryAccess::Write);
  
  auto Result = pipe(pipefd);
  
  // Record the result.
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<int>::type(Result));
  
  // Record the changes to pipefd.
  if (Result == 0) {
    Listener.recordUntypedState(reinterpret_cast<char const *>(pipefd),
                                sizeof(int [2]));
  }
  else {
    Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                                sizeof(errno));
  }
  
  return Result;
}


} // extern "C"
