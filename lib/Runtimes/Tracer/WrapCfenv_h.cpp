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

#include "llvm/Support/CallSite.h"

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


} // extern "C"
