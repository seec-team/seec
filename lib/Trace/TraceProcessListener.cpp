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

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"

#include <cstdio>
#include <utility>
#include <vector>


namespace seec {

namespace trace {


TraceProcessListener::TraceProcessListener(llvm::Module &Module,
                                           ModuleIndex &MIndex,
                                           OutputStreamAllocator &StreamAlloc,
                                           SynchronizedExit &SyncExit)
: StreamAllocator(StreamAlloc),
  OutputEnabled(false),
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
  DataOut(),
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
  InMemoryPointerObjects(),
  DynamicMemoryAllocations(),
  DynamicMemoryAllocationsMutex(),
  StreamsMutex(),
  Streams(),
  StreamsInitial(),
  DirsMutex(),
  Dirs()
{
  // Open traces and enable output.
  traceOpen();
  
  StreamsInitial.emplace_back(reinterpret_cast<uintptr_t>(stdin));
  Streams.streamOpened(stdin,
                       recordData("stdin", std::strlen("stdin") + 1),
                       recordData("r", std::strlen("r") + 1));
  
  StreamsInitial.emplace_back(reinterpret_cast<uintptr_t>(stdout));
  Streams.streamOpened(stdout,
                       recordData("stdout", std::strlen("stdout") + 1),
                       recordData("w", std::strlen("w") + 1));
  
  StreamsInitial.emplace_back(reinterpret_cast<uintptr_t>(stderr));
  Streams.streamOpened(stderr,
                       recordData("stderr", std::strlen("stderr") + 1),
                       recordData("w", std::strlen("w") + 1));
}

TraceProcessListener::~TraceProcessListener() {
  traceWrite();
  traceFlush();
  traceClose();
}


//===----------------------------------------------------------------------===//
// Trace writing control.
//===----------------------------------------------------------------------===//

void TraceProcessListener::traceWrite() {
  if (!OutputEnabled) {
    return;
  }
  
  auto Out = StreamAllocator.getProcessStream(ProcessSegment::Trace);
  if (!Out) {
    assert(false && "couldn't get stream for process trace.");
    return;
  }
  
  uint64_t Version = formatVersion();
  uint32_t NumThreads = NextThreadID - 1;
  
  writeBinary(*Out, Version);
  writeBinary(*Out, Module.getModuleIdentifier());
  writeBinary(*Out, NumThreads);
  writeBinary(*Out, Time);
  writeBinary(*Out, GlobalVariableAddresses);
  writeBinary(*Out, GlobalVariableInitialData);
  writeBinary(*Out, FunctionAddresses);
  writeBinary(*Out, StreamsInitial);
}

void TraceProcessListener::traceFlush() {
  std::lock_guard<std::mutex> Lock(DataOutMutex);
  
  if (DataOut)
    DataOut->flush();
}

void TraceProcessListener::traceClose() {
  std::lock_guard<std::mutex> Lock(DataOutMutex);
  DataOut.reset(nullptr);
  OutputEnabled = false;
}

void TraceProcessListener::traceOpen() {
  std::lock_guard<std::mutex> Lock(DataOutMutex);
  assert(!OutputEnabled && "traceOpen() with OutputEnabled.");
  DataOut =
    StreamAllocator.getProcessStream(ProcessSegment::Data,
                                     llvm::sys::fs::OpenFlags::F_Append);
  OutputEnabled = true;
}


//===----------------------------------------------------------------------===//
// Accessors.
//===----------------------------------------------------------------------===//

seec::Maybe<MemoryArea>
TraceProcessListener::getContainingMemoryArea(uintptr_t Address,
                                              uint32_t RequestingThreadID
                                              ) const {
  // Check global variables.
  {
    auto GlobIt = GlobalVariableLookup.find(Address);
    if (GlobIt != GlobalVariableLookup.end()) {
      auto const Begin = GlobIt->Begin;
      // Range of interval is inclusive: [Begin, End]
      auto const Length = (GlobIt->End - GlobIt->Begin) + 1;
      auto const Permission =
        GlobIt->Value->isConstant() ? MemoryPermission::ReadOnly
                                    : MemoryPermission::ReadWrite;
      
      return MemoryArea(Begin, Length, Permission);
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
        return seec::Maybe<MemoryArea>(Area);
      }
    }
  }
  
  // Check readable/writable regions.
  {
    auto KnownIt = KnownMemory.find(Address);
    if (KnownIt != KnownMemory.end()) {
      // Range of interval is inclusive: [Begin, End]
      auto Length = (KnownIt->End - KnownIt->Begin) + 1;
      return seec::Maybe<MemoryArea>(MemoryArea(KnownIt->Begin,
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
    
  return seec::Maybe<MemoryArea>();
}


//===----------------------------------------------------------------------===//
// Pointer origin tracking.
//===----------------------------------------------------------------------===//

uintptr_t TraceProcessListener::getPointerObject(llvm::Value const *V) const
{
  if (auto const GV = llvm::dyn_cast<llvm::GlobalVariable>(V))
    return getRuntimeAddress(GV);
  if (auto const F = llvm::dyn_cast<llvm::Function>(V))
    return getRuntimeAddress(F);
  if (llvm::isa<llvm::ConstantPointerNull>(V))
    return 0;

#if !defined(NDEBUG)
  llvm_unreachable("don't know how to get pointer object.");
#endif

  return 0;
}

uintptr_t
TraceProcessListener::getInMemoryPointerObject(uintptr_t const PtrLocation)
const
{
  auto const It = InMemoryPointerObjects.find(PtrLocation);

#if 0
  if (It == InMemoryPointerObjects.end())
    llvm_unreachable("couldn't get in-memory pointer's object.");
#endif

  return It != InMemoryPointerObjects.end() ? It->second : 0;
}

void TraceProcessListener::setInMemoryPointerObject(uintptr_t const PtrLocation,
                                                    uintptr_t const Object)
{
  clearInMemoryPointerObjects(MemoryArea(PtrLocation, sizeof(void *)));
  InMemoryPointerObjects[PtrLocation] = Object;
}

void TraceProcessListener::clearInMemoryPointerObjects(MemoryArea const Area)
{
  auto const It = InMemoryPointerObjects.lower_bound(Area.start());
  if (It == InMemoryPointerObjects.end() || It->first >= Area.end())
    return;

  auto const End = InMemoryPointerObjects.lower_bound(Area.end());
  InMemoryPointerObjects.erase(It, End);
}

void TraceProcessListener::copyInMemoryPointerObjects(uintptr_t const From,
                                                      uintptr_t const To,
                                                      std::size_t const Length)
{
  auto const End = To + Length;
  auto const BeginIt = InMemoryPointerObjects.lower_bound(From);
  auto const EndIt   = InMemoryPointerObjects.lower_bound(End);

  // If the memory areas don't intersect then we can copy directly from the
  // source range to the destination range. Otherwise we have to copy into
  // an intermediate container and then copy into the destination range.
  if (!MemoryArea(From,Length).intersects(MemoryArea(To,Length))) {
    clearInMemoryPointerObjects(MemoryArea(To, Length));
    auto const InsertHintIt = InMemoryPointerObjects.lower_bound(To);

    for (auto I = BeginIt; I != EndIt; ++I)
      InMemoryPointerObjects.insert(InsertHintIt,
                                    std::make_pair(To + (I->first - From),
                                                   I->second));
  }
  else {
    std::vector<decltype(InMemoryPointerObjects)::value_type> Objects;
    Objects.reserve(Length);

    for (auto I = BeginIt; I != EndIt; ++I)
      Objects.emplace_back(To + (I->first - From), I->second);

    clearInMemoryPointerObjects(MemoryArea(To, Length));
    auto const InsertHintIt = InMemoryPointerObjects.lower_bound(To);

    for (auto &&Object : Objects)
      InMemoryPointerObjects.insert(InsertHintIt, Object);
  }
}


//===----------------------------------------------------------------------===//
// Memory state tracking.
//===----------------------------------------------------------------------===//

offset_uint TraceProcessListener::recordData(char const *Data, size_t Size) {
  // Don't allow concurrent access to DataOut - multiple threads may wreck
  // the output (and the offsets returned).
  std::lock_guard<std::mutex> DataOutLock(DataOutMutex);
  
  if (!DataOut)
    return 0;

  DataOut->write(Data, Size);

  // Return the offset that the data was written at, which will be used by
  // events to refer to the data.
  auto const WrittenOffset = DataOutOffset;
  DataOutOffset += Size;
  return WrittenOffset;
}


//===----------------------------------------------------------------------===//
// Notifications.
//===----------------------------------------------------------------------===//

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
