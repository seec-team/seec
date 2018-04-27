//===- lib/Trace/ThreadState.cpp ------------------------------------------===//
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

#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// ThreadState
//------------------------------------------------------------------------------

ThreadState::ThreadState(ProcessState &Parent,
                         ThreadTrace const &Trace)
: Parent(Parent),
  Trace(Trace),
  m_NextEvent(llvm::make_unique<EventReference>(Trace.events().begin())),
  ProcessTime(Parent.getProcessTime()),
  ThreadTime(0),
  CallStack(),
  m_CompletedFunctions()
{}

ThreadState::~ThreadState() = default;

void ThreadState::incrementNextEvent() {
  ++*m_NextEvent;
}

void ThreadState::decrementNextEvent() {
  --*m_NextEvent;
}

uint32_t ThreadState::getThreadID() const {
  return Trace.getThreadID();
}

bool ThreadState::isAtStart() const {
  return *m_NextEvent == Trace.events().begin();
}

bool ThreadState::isAtEnd() const {
  return *m_NextEvent == Trace.events().end();
}


//------------------------------------------------------------------------------
// Memory.
//------------------------------------------------------------------------------

seec::Maybe<MemoryArea>
ThreadState::getContainingMemoryArea(stateptr_ty Address) const {
  seec::Maybe<MemoryArea> Area;
  
  for (auto const &FunctionStatePtr : CallStack) {
    Area = FunctionStatePtr->getContainingMemoryArea(Address);
    if (Area.assigned())
      break;
  }
  
  return Area;
}


//------------------------------------------------------------------------------
// Printing.
//------------------------------------------------------------------------------

void printComparable(llvm::raw_ostream &Out, ThreadState const &State)
{
  Out << " Thread #" << (State.getTrace().getThreadID() + 1)
      << " @TT=" << State.getThreadTime()
      << "\n";

  for (auto &Function : State.getCallStack()) {
    printComparable(Out, *Function);
  }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State) {
  Out << " Thread #" << State.getTrace().getThreadID()
      << " @TT=" << State.getThreadTime()
      << "\n";

  for (auto &Function : State.getCallStack()) {
    Out << *Function;
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
