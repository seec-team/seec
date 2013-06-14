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

#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/MakeUnique.hpp"
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
  GlobalVariableStates{},
  UnmappedStaticAreas{},
  ThreadStates{},
  CurrentValueStore{}
{
  for (auto &StatePtr : UnmappedState->getThreadStates())
    ThreadStates.emplace_back(makeUnique<ThreadState>(*this, *StatePtr));
  
  auto const &Mapping = Trace.getMapping();
  auto const &Module = UnmappedState->getModule().getModule();
  auto const &DL = UnmappedState->getDataLayout();
  
  for (auto const &Global : seec::range(Module.global_begin(),
                                        Module.global_end()))
  {
    auto const Address = UnmappedState->getRuntimeAddress(&Global);
    
    if (auto const Mapped = Mapping.getMappedGlobalVariableDecl(&Global)) {
      GlobalVariableStates.emplace_back(makeUnique<GlobalVariable>
                                                  (*this,
                                                   Mapped->getDecl(),
                                                   &Global,
                                                   Address));
    }
    else {
      auto const ElemTy = Global.getType()->getElementType();
      auto const Length = DL.getTypeStoreSize(ElemTy);
      auto const Access = Global.isConstant() ? MemoryPermission::ReadOnly
                                              : MemoryPermission::ReadWrite;
      
      UnmappedStaticAreas.emplace_back(Address, Length, Access);
    }
  }
  
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
    auto const &Globals = this->getGlobalVariables();
    Out << Indentation.getString() << "Globals: " << Globals.size() << "\n";
    
    {
      Indentation.indent();
      for (auto const &Global : Globals) {
        Out << Indentation.getString() << *Global << "\n";
      }
      Indentation.unindent();
    }
    
    // Print dynamic memory allocations.
    auto const Mallocs = this->getDynamicMemoryAllocations();
    if (!Mallocs.empty()) {
      Out << Indentation.getString()
          << "Dynamic Memory Allocations: " << Mallocs.size() << "\n";
      
      {
        Indentation.indent();
        for (auto const &Malloc : Mallocs) {
          Malloc.print(Out, Indentation);
        }
        Indentation.unindent();
      }
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
// ProcessState: Dynamic memory allocations
//===----------------------------------------------------------------------===//

std::vector<MallocState> ProcessState::getDynamicMemoryAllocations() const
{
  std::vector<MallocState> States;
  
  for (auto const &RawMallocPair : UnmappedState->getMallocs()) {
    States.emplace_back(*this, RawMallocPair.second);
  }
  
  return States;
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
