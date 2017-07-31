//===- lib/Runtimes/Tracer/WrapCfenv_h.cpp --------------------------------===//
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

#include "seec/Runtimes/MangleFunction.h"

#include <cmath>


extern "C" {

//===----------------------------------------------------------------------===//
// remquo
//===----------------------------------------------------------------------===//

#define SEEC_REMQUO(TYPE, SUFFIX) \
TYPE SEEC_MANGLE_FUNCTION(remquo ## SUFFIX) (TYPE x, TYPE y, int *quo) {       \
  decltype(SEEC_MANGLE_FUNCTION(remquo ## SUFFIX)) &TheFn = std::remquo;       \
  return seec::SimpleWrapper                                                   \
    <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>                 \
    {seec::runtime_errors::format_selects::CStdFunction::remquo ## SUFFIX}     \
    (TheFn,                                                                    \
     [](TYPE){ return true; }, seec::ResultStateRecorderForNoOp(),             \
     x, y, seec::wrapOutputPointer(quo));                                      \
}

SEEC_REMQUO(float, f)
SEEC_REMQUO(double, )
SEEC_REMQUO(long double, l)

#undef SEEC_REMQUO

//===----------------------------------------------------------------------===//
// nan
//===----------------------------------------------------------------------===//

#define SEEC_NAN(TYPE, SUFFIX) \
TYPE SEEC_MANGLE_FUNCTION(nan ## SUFFIX) (char const * const arg) {            \
  return seec::SimpleWrapper                                                   \
    <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>                  \
    {seec::runtime_errors::format_selects::CStdFunction::nan ## SUFFIX}        \
    (nan ## SUFFIX,                                                            \
     [](TYPE){ return true; }, seec::ResultStateRecorderForNoOp(),             \
     seec::wrapInputCString(arg));                                             \
}

SEEC_NAN(float, f)
SEEC_NAN(double, )
SEEC_NAN(long double, l)

#undef SEEC_NAN

//===----------------------------------------------------------------------===//
// frexp
//===----------------------------------------------------------------------===//

#define SEEC_FREXP(TYPE, SUFFIX) \
TYPE SEEC_MANGLE_FUNCTION(frexp ## SUFFIX) (TYPE arg, int *exp) {              \
  decltype(SEEC_MANGLE_FUNCTION(frexp ## SUFFIX)) &TheFn = std::frexp;         \
  return seec::SimpleWrapper                                                   \
    <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>                 \
    {seec::runtime_errors::format_selects::CStdFunction::frexp ## SUFFIX}      \
    (TheFn,                                                                    \
     [](TYPE){ return true; }, seec::ResultStateRecorderForNoOp(),             \
     arg, seec::wrapOutputPointer(exp));                                       \
}

SEEC_FREXP(float, f)
SEEC_FREXP(double, )
SEEC_FREXP(long double, l)

#undef SEEC_FREXP

//===----------------------------------------------------------------------===//
// modf
//===----------------------------------------------------------------------===//

#define SEEC_MODF(TYPE, SUFFIX) \
TYPE SEEC_MANGLE_FUNCTION(modf ## SUFFIX) (TYPE arg, TYPE *iptr) {             \
  decltype(SEEC_MANGLE_FUNCTION(modf ## SUFFIX)) &TheFn = std::modf;           \
  return seec::SimpleWrapper                                                   \
    <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>                 \
    {seec::runtime_errors::format_selects::CStdFunction::modf ## SUFFIX}       \
    (TheFn,                                                                    \
     [](TYPE){ return true; }, seec::ResultStateRecorderForNoOp(),             \
     arg, seec::wrapOutputPointer(iptr));                                      \
}

SEEC_MODF(float, f)
SEEC_MODF(double, )
SEEC_MODF(long double, l)

#undef SEEC_MODF

} // extern "C"
