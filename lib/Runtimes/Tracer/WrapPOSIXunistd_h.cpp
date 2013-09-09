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

#include "SimpleWrapper.hpp"
#include "Tracer.hpp"

#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include <cinttypes>
#include <type_traits>

#include <unistd.h>


extern "C" {


//===----------------------------------------------------------------------===//
// execl
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execl)
(char const *filename, ...)
{
  // ... = argN..., 0
  
  llvm_unreachable("SeeC: execl not yet implemented.");
  
  return -1;
}


//===----------------------------------------------------------------------===//
// execv
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execv)
(char const *filename, char * const argv[])
{
  using namespace seec::trace;
  
  auto const FSFunction =
    seec::runtime_errors::format_selects::CStdFunction::execve;
  
  auto &ProcessEnv = getProcessEnvironment();
  auto &ProcessListener = ProcessEnv.getProcessListener();
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  
  // Raise an error if there are multiple threads.
  if (ProcessEnv.getThreadLookup().size() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>
                     (format_selects::CStdFunction::execve),
      RunErrorSeverity::Fatal);
  }
  
  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Lock global memory.
  Listener.acquireGlobalMemoryReadLock();
  
  // Use a CStdLibChecker to help check memory.
  CStdLibChecker Checker{Listener, ThreadEnv.getInstructionIndex(), FSFunction};
  
  // Ensure that the filename string is accessible.
  Checker.checkCStringRead(0, filename);
  
  // Ensure that argv is accessible.
  Checker.checkCStringArray(1, argv);
  
  // Write a complete trace before we call execve, because if it succeeds we
  // will no longer control the process.
  auto const TraceEnabled = ProcessListener.traceEnabled();
  
  if (TraceEnabled) {
    ProcessListener.traceWrite();
    ProcessListener.traceFlush();
    ProcessListener.traceClose();
    Listener.traceWrite();
    Listener.traceFlush();
    Listener.traceClose();
  }
  
  // Forward to the default implementation of execve.
  auto Result = execv(filename, argv);
  
  // At this point the execve has failed, so restore tracing.
  if (TraceEnabled) {
    ProcessListener.traceOpen();
    Listener.traceOpen();
  }
  
  return Result;
}


//===----------------------------------------------------------------------===//
// execle
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execle)
(char const *filename, ...)
{
  // ... = argN..., 0, char * const envp[]
  
  llvm_unreachable("SeeC: execle not yet implemented.");
  
  return -1;
}


//===----------------------------------------------------------------------===//
// execve
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execve)
(char const *filename, char * const argv[], char * const envp[])
{
  using namespace seec::trace;
  
  auto const FSFunction =
    seec::runtime_errors::format_selects::CStdFunction::execve;
  
  auto &ProcessEnv = getProcessEnvironment();
  auto &ProcessListener = ProcessEnv.getProcessListener();
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  
  // Raise an error if there are multiple threads.
  if (ProcessEnv.getThreadLookup().size() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>
                     (format_selects::CStdFunction::execve),
      RunErrorSeverity::Fatal);
  }
  
  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Lock global memory.
  Listener.acquireGlobalMemoryReadLock();
  
  // Use a CStdLibChecker to help check memory.
  CStdLibChecker Checker{Listener, ThreadEnv.getInstructionIndex(), FSFunction};
  
  // Ensure that the filename string is accessible.
  Checker.checkCStringRead(0, filename);
  
  // Ensure that argv is accessible.
  Checker.checkCStringArray(1, argv);
  
  // Ensure that envp is accessible.
  Checker.checkCStringArray(2, envp);
  
  // Write a complete trace before we call execve, because if it succeeds we
  // will no longer control the process.
  auto const TraceEnabled = ProcessListener.traceEnabled();
  
  if (TraceEnabled) {
    ProcessListener.traceWrite();
    ProcessListener.traceFlush();
    ProcessListener.traceClose();
    Listener.traceWrite();
    Listener.traceFlush();
    Listener.traceClose();
  }
  
  // Forward to the default implementation of execve.
  auto Result = execve(filename, argv, envp);
  
  // At this point the execve has failed, so restore tracing.
  if (TraceEnabled) {
    ProcessListener.traceOpen();
    Listener.traceOpen();
  }
  
  return Result;
}


//===----------------------------------------------------------------------===//
// execlp
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execlp)
(char const *filename, ...)
{
  // ... = argN..., 0
  
  llvm_unreachable("SeeC: execlp not yet implemented.");
  
  return -1;
}


//===----------------------------------------------------------------------===//
// execvp
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execvp)
(char const *filename, char * const argv[])
{
  using namespace seec::trace;
  
  auto const FSFunction =
    seec::runtime_errors::format_selects::CStdFunction::execve;
  
  auto &ProcessEnv = getProcessEnvironment();
  auto &ProcessListener = ProcessEnv.getProcessListener();
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  
  // Raise an error if there are multiple threads.
  if (ProcessEnv.getThreadLookup().size() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>
                     (format_selects::CStdFunction::execve),
      RunErrorSeverity::Fatal);
  }
  
  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Lock global memory.
  Listener.acquireGlobalMemoryReadLock();
  
  // Use a CStdLibChecker to help check memory.
  CStdLibChecker Checker{Listener, ThreadEnv.getInstructionIndex(), FSFunction};
  
  // Ensure that the filename string is accessible.
  Checker.checkCStringRead(0, filename);
  
  // Ensure that argv is accessible.
  Checker.checkCStringArray(1, argv);
  
  // Write a complete trace before we call execve, because if it succeeds we
  // will no longer control the process.
  auto const TraceEnabled = ProcessListener.traceEnabled();
  
  if (TraceEnabled) {
    ProcessListener.traceWrite();
    ProcessListener.traceFlush();
    ProcessListener.traceClose();
    Listener.traceWrite();
    Listener.traceFlush();
    Listener.traceClose();
  }
  
  // Forward to the default implementation of execve.
  auto Result = execvp(filename, argv);
  
  // At this point the execve has failed, so restore tracing.
  if (TraceEnabled) {
    ProcessListener.traceOpen();
    Listener.traceOpen();
  }
  
  return Result;
}


//===----------------------------------------------------------------------===//
// fork
//===----------------------------------------------------------------------===//

pid_t
SEEC_MANGLE_FUNCTION(fork)
()
{
  using namespace seec::trace;
  
  auto &ProcessEnv = getProcessEnvironment();
  auto &ProcessListener = ProcessEnv.getProcessListener();
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  
  // Raise an error if there are multiple threads.
  if (ProcessEnv.getThreadLookup().size() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>
                     (format_selects::CStdFunction::fork),
      RunErrorSeverity::Fatal);
  }
  
  // Flush output streams prior to the fork, so that information isn't
  // flushed from both processes following the fork.
  ProcessListener.traceFlush();
  Listener.traceFlush();
  
  // Do the fork.
  auto Result = fork();
  
  if (Result == 0) {
    // This is the child process. We need to modify our tracing environment so
    // that we don't interfere with the parent process. Any other threads that
    // are waiting for us will need to update any environment references that
    // they are currently using (alternatively, no other threads should be
    // allowed to have an environment reference at the synchronization point).
    ProcessListener.traceClose();
    Listener.traceClose();
  }
  
  Listener.notifyValue(ThreadEnv.getInstructionIndex(),
                       ThreadEnv.getInstruction(),
                       std::make_unsigned<pid_t>::type(Result));
  
  return Result;
}


//===----------------------------------------------------------------------===//
// pipe
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


//===----------------------------------------------------------------------===//
// unlink
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(unlink)
(char const *pathname)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::unlink}
      (unlink,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(pathname));
}


} // extern "C"
