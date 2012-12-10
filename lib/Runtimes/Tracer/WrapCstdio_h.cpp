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

#include <cstdio>


extern "C" {

int
SEEC_MANGLE_FUNCTION(scanf)
(uint32_t Index, char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();
  
  auto Call = llvm::dyn_cast<llvm::CallInst>(FunIndex.getInstruction(Index));
  assert(Call && "Expected CallInst");
  
  // Check.
  
  // Do.
  llvm_unreachable("scanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

int
SEEC_MANGLE_FUNCTION(fscanf)
(uint32_t Index, FILE *Stream, char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();
  
  auto Call = llvm::dyn_cast<llvm::CallInst>(FunIndex.getInstruction(Index));
  assert(Call && "Expected CallInst");
  
  // Check.
  
  // Do.
  llvm_unreachable("fscanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

int
SEEC_MANGLE_FUNCTION(sscanf)
(uint32_t Index, char const *Buffer, char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();
  
  auto Call = llvm::dyn_cast<llvm::CallInst>(FunIndex.getInstruction(Index));
  assert(Call && "Expected CallInst");
  
  // Check.
  
  // Do.
  llvm_unreachable("sscanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

} // extern "C"
