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

#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Runtimes/MangleFunction.h"

#include <pthread.h>


extern "C" {


//===----------------------------------------------------------------------===//
// pthread_create
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(pthread_create)
(pthread_t *thread,
 pthread_attr_t const *attr,
 void *(*start_routine)(void *),
 void *arg)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::pthread_create}
      (pthread_create,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputPointer(thread),
       seec::wrapInputPointer(attr).setIgnoreNull(true),
       start_routine,
       arg);
}

//===----------------------------------------------------------------------===//
// pthread_join
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(pthread_join)
(pthread_t thread,
 void **value_ptr)
{
  // Use the SimpleWrapper mechanism.
  // TODO: Check if *value_ptr has a determinable origin.
  return
    seec::SimpleWrapper
      <>
      {seec::runtime_errors::format_selects::CStdFunction::pthread_join}
      (pthread_join,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       thread,
       seec::wrapOutputPointer(value_ptr).setIgnoreNull(true)
                                         .setOriginNewValid());
}


} // extern "C"
