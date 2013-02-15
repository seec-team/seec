//===- lib/Clang/MappedProcessState.cpp -----------------------------------===//
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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


//===----------------------------------------------------------------------===//
// ProcessState
//===----------------------------------------------------------------------===//

ProcessState::ProcessState(seec::cm::ProcessTrace const &ForTrace)
: Trace(ForTrace),
  UnmappedState(new seec::trace::ProcessState(ForTrace.getUnmappedTrace(),
                                              ForTrace.getModuleIndex())),
  ThreadStates()
{
  for (auto &StatePtr : UnmappedState->getThreadStates())
    ThreadStates.emplace_back(new seec::cm::ThreadState(*this, *StatePtr));
}

ProcessState::~ProcessState() = default;

uint64_t ProcessState::getProcessTime() const {
  return UnmappedState->getProcessTime();
}

std::size_t ProcessState::getThreadCount() const {
  return UnmappedState->getThreadStateCount();
}

ThreadState &ProcessState::getThread(std::size_t Index) {
  assert(Index < ThreadStates.size());
  return *ThreadStates[Index];
}

ThreadState const &ProcessState::getThread(std::size_t Index) const {
  assert(Index < ThreadStates.size());
  return *ThreadStates[Index];
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State)
{
  Out << "Process State @" << State.getProcessTime() << "\n";
  
  Out << " Threads: " << State.getThreadCount() << "\n";
  
  for (std::size_t i = 0; i < State.getThreadCount(); ++i) {
    Out << "Thread #" << i << ":\n";
    Out << State.getThread(i);
  }
  
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
