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

#include "llvm/IR/CallSite.h"

#include <ctime>


extern "C" {


//===----------------------------------------------------------------------===//
// time
//===----------------------------------------------------------------------===//

std::time_t
SEEC_MANGLE_FUNCTION(time)
(std::time_t *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::time}
      (time,
       [](std::time_t Result){ return Result != std::time_t(-1); },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputPointer(time_ptr).setIgnoreNull(true));
}


//===----------------------------------------------------------------------===//
// asctime
//===----------------------------------------------------------------------===//

char *
SEEC_MANGLE_FUNCTION(asctime)
(std::tm const *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  // TODO: Behaviour is undefined if the output string would be longer than 25
  //       characters.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::asctime}
      .returnPointerIsNewAndValid()
      (asctime,
       [](char *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForStaticInternalCString(
         seec::MemoryPermission::ReadWrite),
       seec::wrapInputPointer(time_ptr));
}


//===----------------------------------------------------------------------===//
// ctime
//===----------------------------------------------------------------------===//

char *
SEEC_MANGLE_FUNCTION(ctime)
(std::time_t const *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::ctime}
      .returnPointerIsNewAndValid()
      (ctime,
       [](char *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForStaticInternalCString(
         seec::MemoryPermission::ReadWrite),
       seec::wrapInputPointer(time_ptr));
}


//===----------------------------------------------------------------------===//
// strftime
//===----------------------------------------------------------------------===//

std::size_t
SEEC_MANGLE_FUNCTION(strftime)
(char *str, std::size_t count, char const *format, std::tm *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::strftime}
      (strftime,
       [](std::size_t Result){ return Result != 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapOutputCString(str).setMaximumSize(count),
       count,
       seec::wrapInputCString(format),
       seec::wrapInputPointer(time_ptr));
}


//===----------------------------------------------------------------------===//
// gmtime
//===----------------------------------------------------------------------===//

std::tm *
SEEC_MANGLE_FUNCTION(gmtime)
(std::time_t const *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::gmtime}
      .returnPointerIsNewAndValid()
      (gmtime,
       [](std::tm *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForStaticInternalObject(
         seec::MemoryPermission::ReadWrite),
       seec::wrapInputPointer(time_ptr));
}


//===----------------------------------------------------------------------===//
// localtime
//===----------------------------------------------------------------------===//

std::tm *
SEEC_MANGLE_FUNCTION(localtime)
(std::time_t const *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::localtime}
      .returnPointerIsNewAndValid()
      (localtime,
       [](std::tm *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForStaticInternalObject(
         seec::MemoryPermission::ReadWrite),
       seec::wrapInputPointer(time_ptr));
}


//===----------------------------------------------------------------------===//
// mktime
//===----------------------------------------------------------------------===//

std::time_t
SEEC_MANGLE_FUNCTION(mktime)
(std::tm *time_ptr)
{
  // Use the SimpleWrapper mechanism.
  return 
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::mktime}
      (mktime,
       [](std::time_t const &Result){ return Result != std::time_t(-1); },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputPointer(time_ptr));
}


} // extern "C"
