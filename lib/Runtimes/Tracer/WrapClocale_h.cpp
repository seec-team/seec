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
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Support/CallSite.h"

#include <clocale>


//===----------------------------------------------------------------------===//
// setlocale
//===----------------------------------------------------------------------===//

extern "C" {

char *
SEEC_MANGLE_FUNCTION(setlocale)
(int category, char const *locale)
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::setlocale}
      (setlocale,
       [](char *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForStaticInternalCString(
         seec::MemoryPermission::ReadOnly),
       category,
       seec::wrapInputCString(locale).setIgnoreNull(true));
}

} // extern "C"


//===----------------------------------------------------------------------===//
// localeconv
//===----------------------------------------------------------------------===//

class ResultStateRecorderForStaticInternalLConv {
  void recordCString(seec::trace::TraceProcessListener &ProcessListener,
                     seec::trace::TraceThreadListener &ThreadListener,
                     char *String)
  {
    if (String == nullptr)
      return;
    
    auto const Address = reinterpret_cast<uintptr_t>(String);
    auto const Length = std::strlen(String) + 1;
    
    ThreadListener.removeKnownMemoryRegion(Address);
    ThreadListener.addKnownMemoryRegion(Address, Length,
                                        seec::MemoryPermission::ReadOnly);
    ThreadListener.recordUntypedState(String, Length);
  }
  
public:
  ResultStateRecorderForStaticInternalLConv() {}
  
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener,
              std::lconv *Value)
  {
    if (Value == nullptr)
      return;
    
    // Record knowledge of the lconv struct.
    auto const Address = reinterpret_cast<uintptr_t>(Value);
    auto const Ptr = reinterpret_cast<char const *>(Value);
    
    ThreadListener.removeKnownMemoryRegion(Address);
    ThreadListener.addKnownMemoryRegion(Address, sizeof(*Value),
                                        seec::MemoryPermission::ReadOnly);
    ThreadListener.recordUntypedState(Ptr, sizeof(*Value));
    
    // Record knowledge of all strings pointed to by the struct's members.
    recordCString(ProcessListener, ThreadListener, Value->decimal_point);
    recordCString(ProcessListener, ThreadListener, Value->thousands_sep);
    recordCString(ProcessListener, ThreadListener, Value->grouping);
    recordCString(ProcessListener, ThreadListener, Value->mon_decimal_point);
    recordCString(ProcessListener, ThreadListener, Value->mon_thousands_sep);
    recordCString(ProcessListener, ThreadListener, Value->mon_grouping);
    recordCString(ProcessListener, ThreadListener, Value->positive_sign);
    recordCString(ProcessListener, ThreadListener, Value->negative_sign);
    recordCString(ProcessListener, ThreadListener, Value->currency_symbol);
    recordCString(ProcessListener, ThreadListener, Value->int_curr_symbol);
  }
};

extern "C" {

std::lconv *
SEEC_MANGLE_FUNCTION(localeconv)
()
{
  // Use the SimpleWrapper mechanism.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::localeconv}
      (localeconv,
       [](std::lconv *Result){ return Result != nullptr; },
       ResultStateRecorderForStaticInternalLConv());
}

} // extern "C"
