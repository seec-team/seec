//===- lib/Runtimes/Tracer/SignalHandling.cpp -----------------------------===//
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

#define _POSIX_C_SOURCE 200809L

// We need this under xcode9, otherwise the presence of _POSIX_C_SOURCE causes
// some functions to not be defined. Those functions are later referenced by
// __threading_support in libc++, breaking the compile.
#if (defined(__APPLE__))
#define _DARWIN_C_SOURCE
#endif

#include "SignalHandling.hpp"

#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceSignalInfo.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/Range.hpp"

#include <cstdint>
#include <initializer_list>

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <signal.h>
#include <string.h>
#include <unistd.h>
#endif


namespace {

struct SignalInfo {
  int const m_Value;
  char const * const m_Name;
};

#define SEEC_SIGNAL_ENTRY(SIG) SignalInfo{SIG, #SIG}

// We only attempt to catch and record signals which would terminate the
// process (according to their default action).
constexpr SignalInfo SignalsToCatch[] = {
#if defined(SIGHUP)
  SEEC_SIGNAL_ENTRY(SIGHUP),
#endif
#if defined(SIGINT)
  SEEC_SIGNAL_ENTRY(SIGINT),
#endif
#if defined(SIGQUIT)
  SEEC_SIGNAL_ENTRY(SIGQUIT),
#endif
#if defined(SIGILL)
  SEEC_SIGNAL_ENTRY(SIGILL),
#endif
#if defined(SIGABRT)
  SEEC_SIGNAL_ENTRY(SIGABRT),
#endif
#if defined(SIGFPE)
  SEEC_SIGNAL_ENTRY(SIGFPE),
#endif
#if defined(SIGSEGV)
  SEEC_SIGNAL_ENTRY(SIGSEGV),
#endif
#if defined(SIGPIPE)
  SEEC_SIGNAL_ENTRY(SIGPIPE),
#endif
#if defined(SIGALRM)
  SEEC_SIGNAL_ENTRY(SIGALRM),
#endif
#if defined(SIGTERM)
  SEEC_SIGNAL_ENTRY(SIGTERM),
#endif
#if defined(SIGUSR1)
  SEEC_SIGNAL_ENTRY(SIGUSR1),
#endif
#if defined(SIGUSR2)
  SEEC_SIGNAL_ENTRY(SIGUSR2),
#endif
#if defined(SIGBUS)
  SEEC_SIGNAL_ENTRY(SIGBUS),
#endif
#if defined(SIGPOLL)
  SEEC_SIGNAL_ENTRY(SIGPOLL),
#endif
#if defined(SIGPROF)
  SEEC_SIGNAL_ENTRY(SIGPROF),
#endif
#if defined(SIGSYS)
  SEEC_SIGNAL_ENTRY(SIGSYS),
#endif
#if defined(SIGTRAP)
  SEEC_SIGNAL_ENTRY(SIGTRAP),
#endif
#if defined(SIGVTALRM)
  SEEC_SIGNAL_ENTRY(SIGVTALRM),
#endif
#if defined(SIGXCPU)
  SEEC_SIGNAL_ENTRY(SIGXCPU),
#endif
#if defined(SIGXFSZ)
  SEEC_SIGNAL_ENTRY(SIGXFSZ),
#endif
};

#undef SEEC_SIGNAL_ENTRY

static seec::trace::OutputStreamAllocator * volatile GlobalOutput;
thread_local uint32_t volatile ThreadID;
thread_local std::atomic<uint64_t> volatile const * volatile ThreadTime;

} // anonymous namespace


#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))

extern "C" {

void SEEC_MANGLE_FUNCTION(__seec_signal_handler)(int sig, siginfo_t *info, void *)
{
  using namespace seec::trace;
  
  if (auto const OutAllocator = GlobalOutput) {
    char const *Name = nullptr;
    for (auto &Def : seec::range(SignalsToCatch)) {
      if (Def.m_Value == sig) {
        Name = Def.m_Name;
        break;
      }
    }
    
    uint32_t const tID = ThreadID;
    
    auto const tTimePtr = ThreadTime;
    uint64_t const tTime = tTimePtr ? tTimePtr->load() : 0;
    
    seec::trace::writeSignalInfo(*OutAllocator,
                                 tID,
                                 tTime,
                                 sig,
                                 Name,
                                 strsignal(sig));
  }
  
  // Attempt to re-raise the signal with the default handling.
  struct sigaction sa;
  sa.sa_handler = SIG_DFL;
  
  if (sigaction(sig, &sa, nullptr)) {
    _exit(1);
  }
  
  raise(sig);
}

} // extern "C"


namespace seec {

namespace trace {

void setupSignalHandling(OutputStreamAllocator * const WithOutput)
{
  GlobalOutput = WithOutput;
  
  struct sigaction act;
  
  for (auto &Def : seec::range(SignalsToCatch)) {
    // These signals cannot be caught.
    if (Def.m_Value == SIGKILL || Def.m_Value == SIGSTOP)
      continue;
    
    if (sigaction(Def.m_Value, nullptr, &act) != 0) {
      continue;
    }
    
    if (act.sa_handler == SIG_DFL) {
      struct sigaction sa;
      sa.sa_sigaction = SEEC_MANGLE_FUNCTION(__seec_signal_handler);
      sa.sa_flags = SA_SIGINFO;
      
      // It's not a disaster if we can't set the action - even if a signal
      // is raised, the generated trace will be usable.
      sigaction(Def.m_Value, &sa, nullptr);
    }
  }
}

void setupThreadForSignalHandling(TraceThreadListener const &ForThread)
{
  auto const &Time = ForThread.getThreadTime();
  assert(Time.is_lock_free());
  
  ThreadID = ForThread.getThreadID();
  ThreadTime = &Time;
}

void teardownThreadForSignalHandling()
{
  ThreadID = 0;
  ThreadTime = nullptr;
}

} // namespace trace (in seec)

} // namespace seec


#else
// Generic implementation: do nothing.

namespace seec {

namespace trace {

void setupSignalHandling(OutputStreamAllocator * const WithOutput) {}

void setupThreadForSignalHandling(TraceThreadListener const &ForThread) {}

void teardownThreadForSignalHandling() {}

} // namespace trace (in seec)

} // namespace seec

#endif
