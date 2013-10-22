//===- lib/Runtimes/Tracer/WrapPOSIXsys_time_h ----------------------------===//
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

#include <sys/time.h>


extern "C" {


//===----------------------------------------------------------------------===//
// gettimeofday
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(gettimeofday)
(struct timeval *tv, struct timezone *tz)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::gettimeofday}
      (gettimeofday,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputPointer(tv).setIgnoreNull(true),
       seec::wrapOutputPointer(tz).setIgnoreNull(true));
}


//===----------------------------------------------------------------------===//
// settimeofday
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(settimeofday)
(struct timeval const *tv, struct timezone const *tz)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::gettimeofday}
      (settimeofday,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputPointer(tv),
       seec::wrapInputPointer(tz));
}


} // extern "C"
