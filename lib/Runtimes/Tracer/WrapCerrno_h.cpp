//===- lib/Runtimes/Tracer/WrapClocale_h.cpp ------------------------------===//
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


extern "C" {

extern int *__errno_location() __attribute__((weak));

int *
SEEC_MANGLE_FUNCTION(__errno_location)
()
{
  assert(__errno_location);

  return seec::SimpleWrapper
          <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
          {seec::runtime_errors::format_selects::CStdFunction::__errno_location}
          .returnPointerIsNewAndValid()
          (__errno_location,
           [](int const * const Result){ return Result != nullptr; },
           seec::ResultStateRecorderForStaticInternalObject{
             seec::MemoryPermission::ReadWrite
           });
}

} // extern "C"
