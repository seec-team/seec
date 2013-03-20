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
#include "seec/Util/Printing.hpp"

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
  ThreadStates(),
  CurrentValueStore()
{
  for (auto &StatePtr : UnmappedState->getThreadStates())
    ThreadStates.emplace_back(new seec::cm::ThreadState(*this, *StatePtr));
  
  cacheClear();
}

ProcessState::~ProcessState() = default;

void ProcessState::cacheClear() {
  // Clear process-level cached information.
  CurrentValueStore = seec::cm::ValueStore::create();
  
  // Clear thread-level cached information.
  for (auto &ThreadPtr : ThreadStates)
    ThreadPtr->cacheClear();
}

void ProcessState::print(llvm::raw_ostream &Out,
                         seec::util::IndentationGuide &Indentation) const
{
  Out << "Process State @" << this->getProcessTime() << "\n";
  
  {
    Indentation.indent();
    
    // Print global variables.
    auto const Globals = this->getGlobalVariables();
    Out << Indentation.getString() << "Globals: " << Globals.size() << "\n";
    
    {
      Indentation.indent();
      for (auto const &Global : Globals) {
        Out << Indentation.getString() << Global << "\n";
      }
      Indentation.unindent();
    }
    
    // Print thread states.
    for (std::size_t i = 0; i < this->getThreadCount(); ++i) {
      Out << Indentation.getString() << "Thread #" << i << ":\n";
      
      {
        Indentation.indent();
        this->getThread(i).print(Out, Indentation);
        Indentation.unindent();
      }
    }
    
    Indentation.unindent();
  }
}

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


//===----------------------------------------------------------------------===//
// ProcessState: Global variables
//===----------------------------------------------------------------------===//

std::vector<GlobalVariable> ProcessState::getGlobalVariables() const
{
  std::vector<GlobalVariable> Globals;
  
  for (auto const &It : Trace.getMapping().getGlobalVariableLookup()) {
    auto const GV = It.second.getGlobal();
    auto const Address = UnmappedState->getRuntimeAddress(GV);
    
    Globals.emplace_back(*this,
                         It.second.getDecl(),
                         GV,
                         Address);
  }
  
  return Globals;
}


//===----------------------------------------------------------------------===//
// ProcessState: Printing
//===----------------------------------------------------------------------===//

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State)
{
  seec::util::IndentationGuide Indent("  ");
  State.print(Out, Indent);
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
