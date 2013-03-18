//===- lib/Clang/MappedThreadState.cpp ------------------------------------===//
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

#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/ThreadState.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// ThreadState
//===----------------------------------------------------------------------===//

ThreadState::ThreadState(ProcessState &WithParent,
                         seec::trace::ThreadState &ForState)
: Parent(WithParent),
  UnmappedState(ForState),
  CallStack()
{
  cacheClear();
}

ThreadState::~ThreadState() = default;

void ThreadState::cacheClear() {
  generateCallStack();
}


//===----------------------------------------------------------------------===//
// Call stack
//===----------------------------------------------------------------------===//

void ThreadState::generateCallStack() {
  CallStack.clear();
  CallStackRefs.clear();
  
  for (auto const &RawFunctionStatePtr : UnmappedState.getCallStack()) {
    CallStack.emplace_back(new FunctionState(*this, *RawFunctionStatePtr));
    CallStackRefs.emplace_back(*CallStack.back());
  }
}


//===----------------------------------------------------------------------===//
// llvm::raw_ostream output
//===----------------------------------------------------------------------===//

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, ThreadState const &State)
{
  Out << "ThreadTime = " << State.getUnmappedState().getThreadTime() << "\n";
  
  Out << "Call Stack:\n";
  for (auto const &Function : State.getCallStack())
    Out << Function;
  
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
