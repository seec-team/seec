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
  CurrentValueStore{},
  Streams{},
  Dirs{}
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
      auto const TheDecl = llvm::dyn_cast<clang::VarDecl>(Mapped->getDecl());
      if (!TheDecl)
        continue;
      
      GlobalVariableStates.emplace_back(makeUnique<GlobalVariable>
                                                  (*this,
                                                   *Mapped,
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
  CurrentValueStore = seec::cm::ValueStore::create(Trace.getMapping());
  Streams.clear();
  Dirs.clear();
  
  // Generate stream information.
  for (auto const &Pair : UnmappedState->getStreams())
    Streams.insert(std::make_pair(Pair.first, StreamState(Pair.second)));
  
  // Generate DIR information.
  for (auto const &Pair : UnmappedState->getDirs())
    Dirs.insert(std::make_pair(Pair.first, DIRState(Pair.second)));
  
  // Clear thread-level cached information.
  for (auto &ThreadPtr : ThreadStates)
    ThreadPtr->cacheClear();
}

void ProcessState::print(llvm::raw_ostream &Out,
                         seec::util::IndentationGuide &Indentation,
                         AugmentationCallbackFn Augmenter)
const
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
    
    // Print streams.
    if (!Streams.empty()) {
      Out << Indentation.getString()
          << "Streams: " << Streams.size() << "\n";
      
      Indentation.indent();
      
      for (auto const &Pair : Streams) {
        Out << Indentation.getString() << Pair.second << "\n";
      }
      
      Indentation.unindent();
    }
    
    // Print dirs.
    if (!Dirs.empty()) {
      Out << Indentation.getString()
          << "DIRs: " << Dirs.size() << "\n";
      
      Indentation.indent();
      
      for (auto const &Pair : Dirs) {
        Out << Indentation.getString() << Pair.second << "\n";
      }
      
      Indentation.unindent();
    }
    
    // Print thread states.
    for (std::size_t i = 0; i < this->getThreadCount(); ++i) {
      Out << Indentation.getString() << "Thread #" << i << ":\n";
      
      {
        Indentation.indent();
        this->getThread(i).print(Out, Indentation, Augmenter);
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

seec::Maybe<std::size_t>
ProcessState::getThreadIndex(seec::cm::ThreadState const &Thread) const
{
  for (std::size_t i = 0; i < ThreadStates.size(); ++i)
    if (ThreadStates[i].get() == &Thread)
      return i;
  
  return seec::Maybe<std::size_t>();
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

bool ProcessState::isStaticallyAllocated(stateptr_ty const Address) const
{
  return UnmappedState->isContainedByGlobalVariable(Address);
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

seec::Maybe<MallocState>
ProcessState::getDynamicMemoryAllocation(stateptr_ty const Address) const
{
  auto const It = UnmappedState->getMallocs().find(Address);
  if (It == UnmappedState->getMallocs().end())
    return seec::Maybe<MallocState>();

  return MallocState{*this, It->second};
}


//===----------------------------------------------------------------------===//
// ProcessState: Streams
//===----------------------------------------------------------------------===//

StreamState const *ProcessState::getStream(stateptr_ty const Address) const
{
  auto const It = Streams.find(Address);
  return It != Streams.end() ? &It->second : nullptr;
}

StreamState const *ProcessState::getStreamStdout() const
{
  auto const &StreamsInitial = UnmappedState->getTrace().getStreamsInitial();
  if (StreamsInitial.size() >= 2)
    return getStream(StreamsInitial[1]);
  return nullptr;
}


//===----------------------------------------------------------------------===//
// ProcessState: DIRs
//===----------------------------------------------------------------------===//

DIRState const *ProcessState::getDIR(stateptr_ty const Address) const
{
  auto const It = Dirs.find(Address);
  return It != Dirs.end() ? &It->second : nullptr;
}


//===----------------------------------------------------------------------===//
// ProcessState: Printing
//===----------------------------------------------------------------------===//

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State)
{
  seec::util::IndentationGuide Indent("  ");
  State.print(Out, Indent, AugmentationCallbackFn{});
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
