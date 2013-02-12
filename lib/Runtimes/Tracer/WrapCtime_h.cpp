//===- lib/Runtimes/Tracer/WrapCtime_h.cpp --------------------------------===//
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

#include "llvm/Support/CallSite.h"

#include <ctime>


extern "C" {

//===----------------------------------------------------------------------===//
// time
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(time)
(std::time_t *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return seec::SimpleWrapper<
          seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
          {seec::runtime_errors::format_selects::CStdFunction::time}
          (time,
           [](std::time_t Result){return Result != std::time_t(-1);},
           seec::wrapOutputPointer(time_ptr).setIgnoreNull(true));
}


} // extern "C"
