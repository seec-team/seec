//===- lib/Runtimes/Tracer/WrapPOSIXdirent_h ------------------------------===//
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

#include <dirent.h>


namespace seec {

//===----------------------------------------------------------------------===//
// WrappedInputDIR
//===----------------------------------------------------------------------===//

class WrappedInputDIR {
  DIR *Value;
  
  bool IgnoreNull;
  
public:
  WrappedInputDIR(DIR *ForValue)
  : Value(ForValue),
    IgnoreNull(false)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedInputDIR &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator DIR *() const { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  /// @}
};

inline WrappedInputDIR wrapInputDIR(DIR *ForValue) {
  return WrappedInputDIR(ForValue);
}

/// \brief WrappedArgumentChecker specialization for WrappedInputDIR.
///
template<>
class WrappedArgumentChecker<WrappedInputDIR>
{
  /// The underlying memory checker.
  seec::trace::DIRChecker &Checker;

public:
  /// \brief Construct a new WrappedArgumentChecker.
  ///
  WrappedArgumentChecker(seec::trace::CIOChecker &WithIOChecker,
                         seec::trace::DIRChecker &WithDIRChecker)
  : Checker(WithDIRChecker)
  {}
  
  /// \brief Check if the given value is OK.
  ///
  bool check(WrappedInputDIR &Value, int const Parameter) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    DIR * const DIRValue = Value;
    
    return Checker.checkDIRIsValid(Parameter, DIRValue);
  }
};

//===----------------------------------------------------------------------===//
// ResultStateRecorderForDIROpen
//===----------------------------------------------------------------------===//

class ResultStateRecorderForDIROpen {
  char const *Dirname;
  
public:
  ResultStateRecorderForDIROpen(char const * const WithDirname)
  : Dirname(WithDirname)
  {}
  
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener,
              DIR const *Value)
  {
    if (Value)
      ThreadListener.recordDirOpen(Value, Dirname);
  }
};

//===----------------------------------------------------------------------===//
// ResultStateRecorderForDIRClose
//===----------------------------------------------------------------------===//

class ResultStateRecorderForDIRClose {
  void const *TheDIR;
  
public:
  ResultStateRecorderForDIRClose(DIR const * const WithDIR)
  : TheDIR(WithDIR)
  {}
  
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener,
              int const Value)
  {
    if (Value == 0)
      ThreadListener.recordDirClose(TheDIR);
  }
};

} // namespace seec


extern "C" {


//===----------------------------------------------------------------------===//
// closedir
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(closedir)
(DIR *dirp)
{
  return
    seec::SimpleWrapper
      <>
      {seec::runtime_errors::format_selects::CStdFunction::closedir}
      (closedir,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForDIRClose(dirp),
       seec::wrapInputDIR(dirp));
}


//===----------------------------------------------------------------------===//
// opendir
//===----------------------------------------------------------------------===//

DIR *
SEEC_MANGLE_FUNCTION(opendir)
(char const *dirname)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::opendir}
      .returnPointerIsNewAndValid()
      (opendir,
       [](DIR const *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForDIROpen(dirname),
       seec::wrapInputCString(dirname));
}


//===----------------------------------------------------------------------===//
// readdir
//===----------------------------------------------------------------------===//

struct dirent *
SEEC_MANGLE_FUNCTION(readdir)
(DIR *dirp)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::readdir}
      .returnPointerIsNewAndValid()
      (readdir,
       [](struct dirent *){ return true; },
       seec::ResultStateRecorderForStaticInternalObject{
        seec::MemoryPermission::ReadWrite
       },
       seec::wrapInputDIR(dirp));
}


//===----------------------------------------------------------------------===//
// rewinddir
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(rewinddir)
(DIR *dirp)
{
  seec::SimpleWrapper
    <>
    {seec::runtime_errors::format_selects::CStdFunction::rewinddir}
    (rewinddir,
     [](){ return true; },
     seec::ResultStateRecorderForNoOp(),
     seec::wrapInputDIR(dirp));
}


//===----------------------------------------------------------------------===//
// seekdir
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(seekdir)
(DIR * const dirp, long int const loc)
{
  seec::SimpleWrapper
    <>
    {seec::runtime_errors::format_selects::CStdFunction::seekdir}
    (seekdir,
     [](){ return true; },
     seec::ResultStateRecorderForNoOp(),
     seec::wrapInputDIR(dirp),
     loc);
}


//===----------------------------------------------------------------------===//
// telldir
//===----------------------------------------------------------------------===//

long int
SEEC_MANGLE_FUNCTION(telldir)
(DIR *dirp)
{
  return
    seec::SimpleWrapper
      <>
      {seec::runtime_errors::format_selects::CStdFunction::telldir}
      (telldir,
       [](long int const){ return true; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputDIR(dirp));
}


} // extern "C"
