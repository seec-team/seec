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

#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/ThreadState.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// ThreadState
//===----------------------------------------------------------------------===//

ThreadState::~ThreadState() = default;

void ThreadState::cacheClear() {
  // Nothing to clear.
}


llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, ThreadState const &State)
{
  Out << "ThreadTime = " << State.getUnmappedState().getThreadTime() << "\n";
  
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
