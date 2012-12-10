//===- lib/Runtimes/Tracer/Tracer.hpp -------------------------------------===//
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

#ifndef SEEC_LIB_RUNTIMES_TRACER_TRACER_HPP
#define SEEC_LIB_RUNTIMES_TRACER_TRACER_HPP


#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Trace/TraceThreadListener.hpp"

#include "llvm/LLVMContext.h"

#include <cassert>
#include <memory>
#include <vector>


namespace seec {

namespace trace {

// Forward-declaration.
class ProcessEnvironment;


/// \brief ThreadEnvironment.
///
class ThreadEnvironment {
  ProcessEnvironment &Process;
  
  TraceThreadListener ThreadTracer;
  
  FunctionIndex *FunIndex;
  
  std::vector<llvm::Function *> Stack;
  
public:
  ThreadEnvironment(ProcessEnvironment &PE);
  
  TraceThreadListener &getThreadListener() { return ThreadTracer; }
  
  FunctionIndex &getFunctionIndex() {
    assert(FunIndex);
    return *FunIndex;
  }
  
  void pushFunction(llvm::Function *Fun);
  
  llvm::Function *popFunction();
};


/// \brief ProcessEnvironment.
///
class ProcessEnvironment {
  llvm::LLVMContext Context;
  
  std::unique_ptr<llvm::Module> Mod;
  
  std::unique_ptr<ModuleIndex> ModIndex;
  
  OutputStreamAllocator StreamAllocator;
  
  seec::SynchronizedExit SyncExit;
  
  std::unique_ptr<TraceProcessListener> ProcessTracer;
  
  std::map<std::thread::id, std::unique_ptr<ThreadEnvironment>> ThreadLookup;
  
public:
  ProcessEnvironment();
  
  decltype(ThreadLookup) &getThreadLookup() { return ThreadLookup; }
  
  llvm::LLVMContext &getContext() { return Context; }
  
  llvm::Module const &getModule() const { return *Mod; }
  
  ModuleIndex &getModuleIndex() { return *ModIndex; }
  
  OutputStreamAllocator &getStreamAllocator() { return StreamAllocator; }
  
  SynchronizedExit &getSynchronizedExit() { return SyncExit; }
  
  TraceProcessListener &getProcessListener() { return *ProcessTracer; }
};


/// \brief Get the global ProcessEnvironment.
///
ProcessEnvironment &getProcessEnvironment();


/// \brief Get the current thread's ThreadEnvironment.
///
ThreadEnvironment &getThreadEnvironment();


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_LIB_RUNTIMES_TRACER_TRACER_HPP
