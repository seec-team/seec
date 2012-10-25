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

ProcessState::ProcessState(ProcessTrace const &Trace,
                           ModuleIndex const &ModIndex)
: Trace(Trace),
  Module(ModIndex),
  DL(&ModIndex.getModule()),
  UpdateMutex(),
  UpdateCV(),
  ProcessTime(0),
  ThreadStates(Trace.getNumThreads()),
  Mallocs(),
  Memory()
{
  // Setup initial memory state for global variables.
  for (auto i = 0; i < Module.getGlobalCount(); ++i) {
    auto const Global = Module.getGlobal(i);
    assert(Global);
    
    auto const ElemTy = Global->getType()->getElementType();
    auto const Size = DL.getTypeStoreSize(ElemTy);
    auto const Data = Trace.getGlobalVariableInitialData(i, Size);
    auto const Start = Trace.getGlobalVariableAddress(i);
    
    Memory.add(MappedMemoryBlock(Start, Size, Data.data()), EventLocation());
  }
  
  // Setup ThreadState objects for each thread.
  auto NumThreads = Trace.getNumThreads();
  for (std::size_t i = 0; i < NumThreads; ++i) {
    ThreadStates[i].reset(new ThreadState(*this, Trace.getThreadTrace(i+1)));
  }
}

void ProcessState::setProcessTime(uint64_t Time) {
  std::vector<std::thread> ThreadStateUpdaters;
  
  for (auto &ThreadStatePtr : getThreadStates()) {
    // Create a new thread of execution that will set the process time of this
    // ThreadState.
    auto ThreadStateRawPtr = ThreadStatePtr.get();
    ThreadStateUpdaters.emplace_back([=](){
                                      ThreadStateRawPtr->setProcessTime(Time);
                                     });
  }
  
  // Wait for all ThreadStates to finish updating.
  for (auto &UpdateThread : ThreadStateUpdaters) {
    UpdateThread.join();
  }
}

ProcessState &ProcessState::operator++() {
  if (ProcessTime == Trace.getFinalProcessTime())
    return *this;
  
  setProcessTime(ProcessTime + 1);
  
  return *this;
}

ProcessState &ProcessState::operator--() {
  if (ProcessTime == 0)
    return *this;
  
  setProcessTime(ProcessTime - 1);
  
  return *this;
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
