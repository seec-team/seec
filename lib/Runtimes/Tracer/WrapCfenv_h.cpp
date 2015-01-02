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
#include "Tracer.hpp"

#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/IR/CallSite.h"

#include <cfenv>


extern "C" {

//===----------------------------------------------------------------------===//
// fegetexceptflag
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(fegetexceptflag)
(std::fexcept_t *flagp, int excepts)
{
  // Use the SimpleWrapper mechanism.
  return seec::SimpleWrapper<
          seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
          {seec::runtime_errors::format_selects::CStdFunction::fegetexceptflag}
          (fegetexceptflag,
           [](int Result){ return Result == 0; },
           seec::ResultStateRecorderForNoOp(),
           seec::wrapOutputPointer(flagp),
           excepts);
}


//===----------------------------------------------------------------------===//
// fesetexceptflag
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(fesetexceptflag)
(std::fexcept_t const *flagp, int excepts)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::fesetexceptflag}
      (fesetexceptflag,
       [](int Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputPointer(flagp),
       excepts);
}


//===----------------------------------------------------------------------===//
// fegetenv
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(fegetenv)
(std::fenv_t *envp)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::fegetenv}
      (fegetenv,
       [](int Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputPointer(envp));
}


//===----------------------------------------------------------------------===//
// fesetenv
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(fesetenv)
(std::fenv_t const *envp)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::fesetenv}
      (fesetenv,
       [](int Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputPointer(envp));
}


//===----------------------------------------------------------------------===//
// feholdexcept
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(feholdexcept)
(std::fenv_t *envp)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::feholdexcept}
      (feholdexcept,
       [](int Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputPointer(envp));
}


//===----------------------------------------------------------------------===//
// feupdateenv
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(feupdateenv)
(std::fenv_t const *envp)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::feupdateenv}
      (feupdateenv,
       [](int Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputPointer(envp));
}


} // extern "C"
