//===- lib/Runtimes/Tracer/WrapPOSIXsys_stat_h ----------------------------===//
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

#include <sys/stat.h>


extern "C" {


//===----------------------------------------------------------------------===//
// chmod
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(chmod)
(const char * const path, mode_t const mode)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::chmod}
      (chmod,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       mode);
}


//===----------------------------------------------------------------------===//
// fchmod
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(fchmod)
(int const fildes, mode_t const mode)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::fchmod}
      (fchmod,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       fildes,
       mode);
}


//===----------------------------------------------------------------------===//
// fstat
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(fstat)
(int const fildes, struct stat * buf)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::fstat}
      (fstat,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       fildes,
       seec::wrapOutputPointer(buf));
}


//===----------------------------------------------------------------------===//
// lstat
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(lstat)
(const char * path, struct stat * buf)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::lstat}
      (lstat,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       seec::wrapOutputPointer(buf));
}


//===----------------------------------------------------------------------===//
// mkdir
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(mkdir)
(const char * const path, mode_t const mode)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::mkdir}
      (mkdir,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       mode);
}


//===----------------------------------------------------------------------===//
// mkfifo
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(mkfifo)
(const char * const path, mode_t const mode)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::mkfifo}
      (mkfifo,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       mode);
}


//===----------------------------------------------------------------------===//
// mknod
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(mknod)
(const char * const path, mode_t const mode, dev_t const dev)
{
  // According to The Open Group Base Specifications (Issue 6), the behaviour
  // of mknod() is unspecified if dev != 0. We might want to check that here to
  // ensure that students are always writing portable code.
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryReadLock>
      {seec::runtime_errors::format_selects::CStdFunction::mknod}
      (mknod,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       mode,
       dev);
}


//===----------------------------------------------------------------------===//
// stat
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(stat)
(const char * path, struct stat * buf)
{
  return
    seec::SimpleWrapper
      <seec::SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>
      {seec::runtime_errors::format_selects::CStdFunction::stat}
      (stat,
       [](int const Result){ return Result == 0; },
       seec::ResultStateRecorderForNoOp(),
       seec::wrapInputCString(path),
       seec::wrapOutputPointer(buf));
}


//===----------------------------------------------------------------------===//
// umask
// Handled by include/seec/Transforms/FunctionsHandled.def
//===----------------------------------------------------------------------===//


} // extern "C"
