//===- lib/Runtimes/Tracer/WrapPOSIXsys_stat_h ----------------------------===//
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

#include <sys/stat.h>


extern "C" {


//===----------------------------------------------------------------------===//
// mkdir
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(mkdir)
(const char * const path, mode_t const mode)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::mkdir}
      (mkdir,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       mode);
}


//===----------------------------------------------------------------------===//
// stat
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(stat)
(const char * path, struct stat * buf)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::stat}
      (stat,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       seec::wrapOutputPointer(buf));
}


} // extern "C"
