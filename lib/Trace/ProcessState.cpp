//===- lib/Trace/ProcessState.cpp -----------------------------------------===//
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

#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/Support/raw_ostream.h"

#include <thread>
#include <functional>

namespace seec {

namespace trace {

//------------------------------------------------------------------------------
// ProcessState
//------------------------------------------------------------------------------

ProcessState::ProcessState(std::shared_ptr<ProcessTrace const> TracePtr,
                           std::shared_ptr<ModuleIndex const> ModIndexPtr)
: Trace(std::move(TracePtr)),
  Module(std::move(ModIndexPtr)),
  DL(&(Module->getModule())),
  UpdateMutex(),
  UpdateCV(),
  ProcessTime(0),
  ThreadStates(Trace->getNumThreads()),
  Mallocs(),
  Memory()
{
  // Setup initial memory state for global variables.
  for (std::size_t i = 0; i < Module->getGlobalCount(); ++i) {
    auto const Global = Module->getGlobal(i);
    assert(Global);
    
    auto const ElemTy = Global->getType()->getElementType();
    auto const Size = DL.getTypeStoreSize(ElemTy);
    auto const Data = Trace->getGlobalVariableInitialData(i, Size);
    auto const Start = Trace->getGlobalVariableAddress(i);
    
    Memory.add(MappedMemoryBlock(Start, Size, Data.data()), EventLocation());
  }
  
  // Setup ThreadState objects for each thread.
  auto NumThreads = Trace->getNumThreads();
  for (std::size_t i = 0; i < NumThreads; ++i) {
    ThreadStates[i].reset(new ThreadState(*this, Trace->getThreadTrace(i+1)));
  }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State) {
  Out << "Process @" << State.getProcessTime() << "\n";
  
  Out << " Dynamic Allocations: " << State.getMallocs().size() << "\n";
  
  Out << " Memory State Fragments: "
      << State.getMemory().getNumberOfFragments() << "\n";
  Out << State.getMemory();
  
  for (auto &ThreadStatePtr : State.getThreadStates()) {
    Out << *ThreadStatePtr;
  }
  
  return Out;
}

} // namespace trace (in seec)

} // namespace seec
