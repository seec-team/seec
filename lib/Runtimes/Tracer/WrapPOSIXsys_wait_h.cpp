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

#include "SimpleWrapper.hpp"
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
  return seec::SimpleWrapper
          <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
          {seec::runtime_errors::format_selects::CStdFunction::wait}
          (wait,
           [](int const Result){ return Result != -1; },
           seec::ResultStateRecorderForNoOp(),
           seec::wrapOutputPointer(stat_loc).setIgnoreNull(true));
}


//===----------------------------------------------------------------------===//
// waitpid
//===----------------------------------------------------------------------===//

pid_t
SEEC_MANGLE_FUNCTION(waitpid)
(pid_t pid, int *status, int options)
{
  return seec::SimpleWrapper
          <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
          {seec::runtime_errors::format_selects::CStdFunction::waitpid}
          (waitpid,
           [](int const Result){ return Result != -1; },
           seec::ResultStateRecorderForNoOp(),
           pid,
           seec::wrapOutputPointer(status).setIgnoreNull(true),
           options);
}


} // extern "C"
