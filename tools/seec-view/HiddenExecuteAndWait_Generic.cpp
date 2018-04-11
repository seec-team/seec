//===- tools/seec-trace-view/HiddenExecuteAndWait_Generic.cpp -------------===//
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

#include "llvm/Support/Program.h"

#include "HiddenExecuteAndWait.hpp"

int HiddenExecuteAndWait(llvm::StringRef Program,
                         const char **Args,
                         const char **EnvPtr,
                         std::string *ErrorMsg,
                         bool *ExecFailed)
{
  return llvm::sys::ExecuteAndWait(Program, Args, EnvPtr,
                                   /* redirects */ {}, /* wait */ 0,
                                   /* mem */ 0, ErrorMsg, ExecFailed);
}
