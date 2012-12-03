//===- lib/Trace/TraceProcessListener.cpp ---------------------------------===//
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

#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Trace/TraceThreadListener.hpp"

#include "llvm/DerivedTypes.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Type.h"


namespace seec {

namespace trace {


TraceProcessListener::TraceProcessListener(llvm::Module &Module,
                                           ModuleIndex &MIndex,
                                           OutputStreamAllocator &StreamAlloc,
                                           SynchronizedExit &SyncExit)
: StreamAllocator(StreamAlloc),
  SyncExit(SyncExit),
  Module(Module),
  DL(&Module),
  MIndex(MIndex),
  DetectCallsLookup(),
  GlobalVariableAddresses(MIndex.getGlobalCount()),
  GlobalVariableLookup(),
  GlobalVariableInitialData(MIndex.getGlobalCount()),
  FunctionAddresses(MIndex.getFunctionCount()),
  FunctionLookup(),
  DataOut(StreamAlloc.getProcessStream(ProcessSegment::Data)),
  DataOutOffset(0),
  DataOutMutex(),
  Time(0),
  NextThreadID(1),
  ActiveThreadListeners(),
  TraceThreadListenerMutex(),
  GlobalMemoryMutex(),
  TraceMemoryMutex(),
  TraceMemory(),
  KnownMemory(),
  DynamicMemoryAllocations(),
  DynamicMemoryAllocationsMutex(),
  StreamsMutex(),
  Streams()
{
  Streams.streamOpened(stdout);
  Streams.streamOpened(stderr);
  Streams.streamOpened(stdin);
}

TraceProcessListener::~TraceProcessListener() {
  auto Out = StreamAllocator.getProcessStream(ProcessSegment::Trace);
  if (!Out)
    return;
    
  uint64_t Version = formatVersion();
  uint32_t NumThreads = NextThreadID - 1;
    
  writeBinary(*Out, Version);
  writeBinary(*Out, Module.getModuleIdentifier());
  writeBinary(*Out, NumThreads);
  // writeBinary(*Out, Time.load());
  writeBinary(*Out, Time);
  writeBinary(*Out, GlobalVariableAddresses);
  writeBinary(*Out, GlobalVariableInitialData);
  writeBinary(*Out, FunctionAddresses);
}

seec::util::Maybe<MemoryArea>
TraceProcessListener::getContainingMemoryArea(uintptr_t Address,
                                              uint32_t RequestingThreadID
                                              ) const {
  // Check global variables.
  {
    auto GlobIt = GlobalVariableLookup.find(Address);
    if (GlobIt != GlobalVariableLookup.end()) {
      auto Begin = GlobIt->Begin;
      // Range of interval is inclusive: [Begin, End]
      auto Length = (GlobIt->End - GlobIt->Begin) + 1;
      return seec::util::Maybe<MemoryArea>(MemoryArea(Begin, Length));
    }
  }
  
  // Check dynamic memory allocations.
  {
    std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);
      
    auto DynIt = DynamicMemoryAllocations.upper_bound(Address);
    if (DynIt != DynamicMemoryAllocations.begin()) {
      --DynIt; // DynIt's allocation address is now <= Address.
        
      auto Area = DynIt->second.area();
      if (Area.contains(Address)) {
        return seec::util::Maybe<MemoryArea>(Area);
      }
    }
  }
  
  // Check readable/writable regions.
  {
    auto KnownIt = KnownMemory.find(Address);
    if (KnownIt != KnownMemory.end()) {
      // Range of interval is inclusive: [Begin, End]
      auto Length = (KnownIt->End - KnownIt->Begin) + 1;
      return seec::util::Maybe<MemoryArea>(MemoryArea(KnownIt->Begin,
                                                      Length,
                                                      KnownIt->Value));
    }
  }
    
  // Check other threads.
  {
    std::lock_guard<std::mutex> Lock(TraceThreadListenerMutex);
      
    for (auto const &Lookup : ActiveThreadListeners) {
      if (Lookup.first != RequestingThreadID) {
        auto MaybeArea = Lookup.second->getContainingMemoryArea(Address);
        if (MaybeArea.assigned()) {
          return MaybeArea;
        }
      }
    }
  }
    
  return seec::util::Maybe<MemoryArea>();
}

offset_uint TraceProcessListener::recordData(char const *Data, size_t Size) {
  // don't allow concurrent access to DataOut - multiple threads may wreck
  // the output (and the offsets returned).
  std::lock_guard<std::mutex> DataOutLock(DataOutMutex);

  DataOut->write(Data, Size);

  // return the offset that the data was written at, which will be used by
  // events to refer to the data.
  auto WrittenOffset = DataOutOffset;
  DataOutOffset += Size;
  return WrittenOffset;
}

void TraceProcessListener::notifyGlobalVariable(uint32_t Index,
                                                llvm::GlobalVariable const *GV,
                                                void const *Address) {
  // GlobalVariable to Address lookup.
  GlobalVariableAddresses[Index] = (uintptr_t)Address;
  
  // Address range to GlobalVariable lookup.
  llvm::Type *ElemTy = GV->getType()->getElementType();

  // Make a closed interval [Start, End].
  uintptr_t Start = reinterpret_cast<uintptr_t>(Address);
  auto Length = DL.getTypeStoreSize(ElemTy);
  uintptr_t End = Start + (Length - 1);

  GlobalVariableLookup.insert(Start, End, GV);
  
  // Set the initial memory state appropriately.
  TraceMemory.add(Start,
                  Length,
                  initialDataThreadID(),
                  Index, // StateRecordOffset (overloaded for GV index).
                  initialDataProcessTime());
  
  auto Offset = recordData(reinterpret_cast<char const *>(Address), Length);
  
  GlobalVariableInitialData[Index] = Offset;
}

void TraceProcessListener::notifyFunction(uint32_t Index,
                                          llvm::Function const *F,
                                          void const *Addr) {
  // Function to Address lookup
  FunctionAddresses[Index] = (uintptr_t)Addr;
  
  // Address to Function lookup
  FunctionLookup.insert(std::make_pair((uintptr_t)Addr, F));
  
  // Lookup for C standard library functions
  DetectCallsLookup.Set(F->getName(), Addr);
}

} // namespace trace (in seec)

} // namespace seec
