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
       seec::ResultStateRecorderForNoOp(),
       dirp);
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
      (opendir,
       [](DIR const *Result){ return Result != nullptr; },
       seec::ResultStateRecorderForNoOp(),
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
      (readdir,
       [](struct dirent *){ return true; },
       seec::ResultStateRecorderForStaticInternalObject{
        seec::MemoryPermission::ReadWrite
       },
       dirp);
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
     dirp);
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
     dirp,
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
       dirp);
}


} // extern "C"
