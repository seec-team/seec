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

#include <cerrno>
#include <cinttypes>
#include <type_traits>

#include <unistd.h>


/// \brief Close a single-threaded trace on construction, but allows for
///        reopening.
///
class SpeculativeTraceClose {
  seec::trace::TraceProcessListener &Process;

  seec::trace::TraceThreadListener &Thread;

  bool const WasEnabled;

public:
  SpeculativeTraceClose(seec::trace::TraceProcessListener &WithProcess,
                        seec::trace::TraceThreadListener &WithThread)
  : Process(WithProcess),
    Thread(WithThread),
    WasEnabled(Process.traceEnabled())
  {
    if (WasEnabled) {
      Process.traceWrite();
      Process.traceFlush();
      Process.traceClose();
      Thread.traceWrite();
      Thread.traceFlush();
      Thread.traceClose();
    }
  }

  void reopen()
  {
    if (WasEnabled) {
      Process.traceOpen();
      Thread.traceOpen();
    }
  }
};


extern "C" {


//===----------------------------------------------------------------------===//
// close
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(close)
(int const fildes)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <>
      {seec::runtime_errors::format_selects::CStdFunction::close}
      (close,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       fildes);
}


//===----------------------------------------------------------------------===//
// execl
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execl)
(char const *filename, ...)
{
  using namespace seec::trace;
  using namespace seec::runtime_errors;

  auto const FSFunction = format_selects::CStdFunction::execl;

  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();

  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();

  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });

  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1)
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>(FSFunction),
      RunErrorSeverity::Fatal,
      InstructionIndex);

  Listener.acquireGlobalMemoryReadLock();

  CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};

  // Ensure that the filename string is accessible.
  Checker.checkCStringRead(0, filename);

  // Ensure that each argument is accessible and correctly typed, and that the
  // list is NULL terminated.
  detect_calls::VarArgList<TraceThreadListener> const
    VarArgs{Listener, llvm::CallSite(Instruction), 1};

  std::vector<char *> ExtractedArgs;

  for (unsigned i = 0; i < VarArgs.size(); ++i) {
    auto const MaybePtr = VarArgs.getAs<char *>(i);
    if (MaybePtr.assigned<char *>()) {
      auto const Ptr = MaybePtr.get<char *>();
      ExtractedArgs.push_back(Ptr);

      if (Ptr != nullptr) {
        if (i + 1 < VarArgs.size()) {
          // Ensure that the pointer refers to a valid C string.
          Checker.checkCStringRead(VarArgs.offset() + i, Ptr);
        }
        else {
          // Raise an error because the list was not NULL terminated.
          Listener.handleRunError(
            createRunError<RunErrorType::VarArgsNonTerminated>(FSFunction)
              ->addAdditional(
                createRunError<RunErrorType::InfoCStdFunctionParameter>
                              (FSFunction, VarArgs.offset() + i)),
            RunErrorSeverity::Fatal,
            InstructionIndex);
        }
      }
      else {
        // Raise a warning if there are superfluous arguments.
        if (i + 1 < VarArgs.size()) {
          Listener.handleRunError(
            createRunError<RunErrorType::VarArgsPostTerminator>(FSFunction)
              ->addAdditional(
                createRunError<RunErrorType::InfoCStdFunctionParameter>
                              (FSFunction, VarArgs.offset() + i + 1)),
            RunErrorSeverity::Warning,
            InstructionIndex);
        }

        break;
      }
    }
    else {
      // Raise an error because the argument has an incorrect type.
      Listener.handleRunError(
        createRunError<RunErrorType::VarArgsExpectedCharPointer>(FSFunction)
          ->addAdditional(
            createRunError<RunErrorType::InfoCStdFunctionParameter>
                          (FSFunction, VarArgs.offset() + i)),
        RunErrorSeverity::Fatal,
        InstructionIndex);
    }
  }

  // Closes trace and restores if destructed.
  SpeculativeTraceClose STC(ProcessListener, Listener);
  auto const Result = execv(filename, ExtractedArgs.data());
  STC.reopen();
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));

  return Result;
}


//===----------------------------------------------------------------------===//
// execlp
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execlp)
(char const *filename, ...)
{
  using namespace seec::trace;
  using namespace seec::runtime_errors;

  auto const FSFunction = format_selects::CStdFunction::execlp;

  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();

  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();

  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });

  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1)
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>(FSFunction),
      RunErrorSeverity::Fatal,
      InstructionIndex);

  Listener.acquireGlobalMemoryReadLock();

  CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};

  // Ensure that the filename string is accessible.
  Checker.checkCStringRead(0, filename);

  // Ensure that each argument is accessible and correctly typed, and that the
  // list is NULL terminated.
  detect_calls::VarArgList<TraceThreadListener> const
    VarArgs{Listener, llvm::CallSite(Instruction), 1};

  std::vector<char *> ExtractedArgs;

  for (unsigned i = 0; i < VarArgs.size(); ++i) {
    auto const MaybePtr = VarArgs.getAs<char *>(i);
    if (MaybePtr.assigned<char *>()) {
      auto const Ptr = MaybePtr.get<char *>();
      ExtractedArgs.push_back(Ptr);

      if (Ptr != nullptr) {
        if (i + 1 < VarArgs.size()) {
          // Ensure that the pointer refers to a valid C string.
          Checker.checkCStringRead(VarArgs.offset() + i, Ptr);
        }
        else {
          // Raise an error because the list was not NULL terminated.
          Listener.handleRunError(
            createRunError<RunErrorType::VarArgsNonTerminated>(FSFunction)
              ->addAdditional(
                createRunError<RunErrorType::InfoCStdFunctionParameter>
                              (FSFunction, VarArgs.offset() + i)),
            RunErrorSeverity::Fatal,
            InstructionIndex);
        }
      }
      else {
        // Raise a warning if there are superfluous arguments.
        if (i + 1 < VarArgs.size()) {
          Listener.handleRunError(
            createRunError<RunErrorType::VarArgsPostTerminator>(FSFunction)
              ->addAdditional(
                createRunError<RunErrorType::InfoCStdFunctionParameter>
                              (FSFunction, VarArgs.offset() + i + 1)),
            RunErrorSeverity::Warning,
            InstructionIndex);
        }

        break;
      }
    }
    else {
      // Raise an error because the argument has an incorrect type.
      Listener.handleRunError(
        createRunError<RunErrorType::VarArgsExpectedCharPointer>(FSFunction)
          ->addAdditional(
            createRunError<RunErrorType::InfoCStdFunctionParameter>
                          (FSFunction, VarArgs.offset() + i)),
        RunErrorSeverity::Fatal,
        InstructionIndex);
    }
  }

  // Closes trace and restores if destructed.
  SpeculativeTraceClose STC(ProcessListener, Listener);
  auto const Result = execvp(filename, ExtractedArgs.data());
  STC.reopen();
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));

  return Result;
}


//===----------------------------------------------------------------------===//
// execle
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execle)
(char const *filename, ...)
{
  using namespace seec::trace;
  using namespace seec::runtime_errors;

  auto const FSFunction = format_selects::CStdFunction::execle;

  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();

  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();

  // Interact with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });

  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1)
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>(FSFunction),
      RunErrorSeverity::Fatal,
      InstructionIndex);

  Listener.acquireGlobalMemoryReadLock();

  CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};

  // Ensure that the filename string is accessible.
  Checker.checkCStringRead(0, filename);

  // Ensure that each argument is accessible and correctly typed, and that the
  // list is NULL terminated.
  detect_calls::VarArgList<TraceThreadListener> const
    VarArgs{Listener, llvm::CallSite(Instruction), 1};

  std::vector<char *> ExtractedArgs;
  char * const *EnvP = nullptr;
  unsigned i;

  for (i = 0; i < VarArgs.size(); ++i) {
    auto const MaybePtr = VarArgs.getAs<char *>(i);
    if (MaybePtr.assigned<char *>()) {
      auto const Ptr = MaybePtr.get<char *>();
      ExtractedArgs.push_back(Ptr);

      if (Ptr == nullptr)
        break;
      
      if (i + 1 < VarArgs.size()) {
        // Ensure that the pointer refers to a valid C string.
        Checker.checkCStringRead(VarArgs.offset() + i, Ptr);
      }
      else {
        // Raise an error because the list was not NULL terminated.
        Listener.handleRunError(
          createRunError<RunErrorType::VarArgsNonTerminated>(FSFunction)
            ->addAdditional(
              createRunError<RunErrorType::InfoCStdFunctionParameter>
                            (FSFunction, VarArgs.offset() + i)),
          RunErrorSeverity::Fatal,
          InstructionIndex);
      }
    }
    else {
      // Raise an error because the argument has an incorrect type.
      Listener.handleRunError(
        createRunError<RunErrorType::VarArgsExpectedCharPointer>(FSFunction)
          ->addAdditional(
            createRunError<RunErrorType::InfoCStdFunctionParameter>
                          (FSFunction, VarArgs.offset() + i)),
        RunErrorSeverity::Fatal,
        InstructionIndex);
    }
  }
  
  // Now get the envp pointer, which should be the last argument.
  ++i;
  
  if (i >= VarArgs.size()) {
    Listener.handleRunError(
      *createRunError<RunErrorType::VarArgsInsufficient>
                     (FSFunction, i + 1, VarArgs.size()),
      RunErrorSeverity::Fatal,
      InstructionIndex);
  }
  
  auto const MaybeEnvP = VarArgs.getAs<char * const *>(i);
  if (MaybeEnvP.assigned<char * const *>()) {
    EnvP = MaybeEnvP.get<char * const *>();
    Checker.checkCStringArray(VarArgs.offset() + i, EnvP);
  }
  else {
    // Raise an error because the argument has an incorrect type.
    Listener.handleRunError(
      createRunError<RunErrorType::VarArgsExpectedCStringArray>(FSFunction)
        ->addAdditional(
          createRunError<RunErrorType::InfoCStdFunctionParameter>
                        (FSFunction, VarArgs.offset() + i)),
      RunErrorSeverity::Fatal,
      InstructionIndex);
  }
  
  if (i + 1 < VarArgs.size()) {
    Listener.handleRunError(
      *createRunError<RunErrorType::VarArgsSuperfluous>
                     (FSFunction, i + 1, VarArgs.size()),
      RunErrorSeverity::Fatal,
      InstructionIndex);
  }

  // Closes trace and restores if destructed.
  SpeculativeTraceClose STC(ProcessListener, Listener);
  auto const Result = execve(filename, ExtractedArgs.data(), EnvP);
  STC.reopen();
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));

  return Result;
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
    seec::runtime_errors::format_selects::CStdFunction::execv;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();
  
  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>(FSFunction),
      RunErrorSeverity::Fatal,
      InstructionIndex);
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
  
  // Write a complete trace before we call execv, because if it succeeds we
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
  
  // Forward to the default implementation of execv.
  auto Result = execv(filename, argv);
  
  // At this point the execv has failed, so restore tracing.
  if (TraceEnabled) {
    ProcessListener.traceOpen();
    Listener.traceOpen();
  }
  
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));
  
  return Result;
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
    seec::runtime_errors::format_selects::CStdFunction::execvp;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();
  
  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>(FSFunction),
      RunErrorSeverity::Fatal,
      InstructionIndex);
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
  
  // Write a complete trace before we call execvp, because if it succeeds we
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
  
  // Forward to the default implementation of execvp.
  auto Result = execvp(filename, argv);
  
  // At this point the execvp has failed, so restore tracing.
  if (TraceEnabled) {
    ProcessListener.traceOpen();
    Listener.traceOpen();
  }
  
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));
  
  return Result;
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
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();
  
  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>
                     (format_selects::CStdFunction::execve),
      RunErrorSeverity::Fatal,
      InstructionIndex);
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
  
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));
  
  return Result;
}


#if 0
//===----------------------------------------------------------------------===//
// execvpe
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(execvpe)
(char const *filename, char * const argv[], char * const envp[])
{
  using namespace seec::trace;
  
  auto const FSFunction =
    seec::runtime_errors::format_selects::CStdFunction::execvpe;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto &ProcessListener = ThreadEnv.getProcessEnvironment()
                                   .getProcessListener();
  
  auto const Instruction = ThreadEnv.getInstruction();
  auto const InstructionIndex = ThreadEnv.getInstructionIndex();
  
  // Raise an error if there are multiple threads.
  if (ProcessListener.countThreadListeners() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>(FSFunction),
      RunErrorSeverity::Fatal,
      InstructionIndex);
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
  
  // Write a complete trace before we call execvpe, because if it succeeds we
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
  
  // Forward to the default implementation of execvpe.
  auto Result = execvpe(filename, argv, envp);
  
  // At this point the execvpe has failed, so restore tracing.
  if (TraceEnabled) {
    ProcessListener.traceOpen();
    Listener.traceOpen();
  }
  
  Listener.notifyValue(InstructionIndex,
                       Instruction,
                       std::make_unsigned<decltype(Result)>::type(Result));
  Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                              sizeof(errno));
  
  return Result;
}
#endif


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
  if (ProcessListener.countThreadListeners() > 1) {
    using namespace seec::runtime_errors;
    
    Listener.handleRunError(
      *createRunError<RunErrorType::UnsafeMultithreaded>
                     (format_selects::CStdFunction::fork),
      RunErrorSeverity::Fatal,
      ThreadEnv.getInstructionIndex());
  }
  
  // Flush output streams prior to the fork, so that information isn't
  // flushed from both processes following the fork.
  auto const TraceEnabled = ProcessListener.traceEnabled();
  if (TraceEnabled) {
    ProcessListener.traceFlush();
    Listener.traceFlush();
  }
  
  // Do the fork.
  auto Result = fork();
  
  if (Result == 0) {
    // This is the child process. We need to modify our tracing environment so
    // that we don't interfere with the parent process. Any other threads that
    // are waiting for us will need to update any environment references that
    // they are currently using (alternatively, no other threads should be
    // allowed to have an environment reference at the synchronization point).
    if (TraceEnabled) {
      ProcessListener.traceClose();
      Listener.traceClose();
    }
  }
  
  Listener.notifyValue(ThreadEnv.getInstructionIndex(),
                       ThreadEnv.getInstruction(),
                       std::make_unsigned<pid_t>::type(Result));
  
  if (Result == -1)
    Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                                sizeof(errno));
  
  return Result;
}


//===----------------------------------------------------------------------===//
// getcwd
//===----------------------------------------------------------------------===//

char *
SEEC_MANGLE_FUNCTION(getcwd)
(char *Buffer, size_t Size)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::getcwd}
      .returnPointerIsNewAndValid()
      (getcwd,
       [](char const * const Result){ return Result != nullptr; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputCString(Buffer).setMaximumSize(Size),
       Size);
}


//===----------------------------------------------------------------------===//
// getopt
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(getopt)
(int const argc, char * const argv[], char const * const optstring)
{
  // Use the SimpleWrapper mechanism.
  // int opterr
  // int optopt
  // int optind
  // char *optarg
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::getopt}
      .trackGlobal(opterr)
      .trackGlobal(optopt)
      .trackGlobal(optind)
      .trackGlobal(optarg)
      (getopt,
       [](int const){ return true; },
       seec::ResultStateRecorderForNoOp(),
       argc,
       seec::wrapInputCStringArray(argv),
       seec::wrapInputCString(optstring));
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
// read
//===----------------------------------------------------------------------===//

ssize_t
SEEC_MANGLE_FUNCTION(read)
(int const fildes, void * const buf, size_t const nbyte)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::read}
      (read,
       [](ssize_t const Result){ return Result >= 0; },
       seec::ResultStateRecorderForNoOp(),
       fildes,
       seec::wrapOutputPointer(buf).setSize(nbyte),
       nbyte);
}


//===----------------------------------------------------------------------===//
// rmdir
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(rmdir)
(char const * const path)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::rmdir}
      (rmdir,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path));
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
