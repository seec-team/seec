//===- lib/Runtimes/Tracer/WrapCstdio_h.cpp -------------------------------===//
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

#include "Tracer.hpp"

#include "seec/Runtimes/MangleFunction.h"

#include "llvm/Support/CallSite.h"

#include <cstdio>


extern "C" {

int
SEEC_MANGLE_FUNCTION(scanf)
(char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Call = llvm::CallSite(ThreadEnv.getInstruction());
  assert(Call && "expected call or invoke instruction.");
  
  // TODO: Check and do.
  
  // Do.
  llvm_unreachable("scanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

int
SEEC_MANGLE_FUNCTION(fscanf)
(FILE *Stream, char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Call = llvm::CallSite(ThreadEnv.getInstruction());
  assert(Call && "expected call or invoke instruction.");
    
  // TODO: Check and do.
  
  // Do.
  llvm_unreachable("fscanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

int
SEEC_MANGLE_FUNCTION(sscanf)
(char const *Buffer, char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Call = llvm::CallSite(ThreadEnv.getInstruction());
  assert(Call && "expected call or invoke instruction.");
    
  // TODO: Check and do.
  
  // Do.
  llvm_unreachable("sscanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

} // extern "C"
