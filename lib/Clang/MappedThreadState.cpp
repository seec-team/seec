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
#include "seec/Util/Printing.hpp"

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

void ThreadState::print(llvm::raw_ostream &Out,
                        seec::util::IndentationGuide &Indentation) const
{
  // Basic information.
  Out << Indentation.getString()
      << "ThreadTime = " << this->getUnmappedState().getThreadTime() << "\n";
  
  // The call stack.
  Out << Indentation.getString() << "Call Stack:\n";
  
  {
    Indentation.indent();
    
    for (seec::cm::FunctionState const &Function : this->getCallStack()) {
      Function.print(Out, Indentation);
    }
    
    Indentation.unindent();
  }
}


//===----------------------------------------------------------------------===//
// Access underlying information.
//===----------------------------------------------------------------------===//

uint32_t ThreadState::getThreadID() const {
  return UnmappedState.getThreadID();
}


//===----------------------------------------------------------------------===//
// Queries.
//===----------------------------------------------------------------------===//

bool ThreadState::isAtStart() const {
  return UnmappedState.isAtStart();
}

bool ThreadState::isAtEnd() const {
  return UnmappedState.isAtEnd();
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
  seec::util::IndentationGuide Indent("  ");
  State.print(Out, Indent);
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
