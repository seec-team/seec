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

#define SEEC_DEBUG_IMPO 0


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
  RunErrorCallback(),
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
  EnvironSetupOnceFlag(),
  GlobalMemoryMutex(),
  TraceMemoryMutex(),
  TraceMemory(),
  KnownMemory(),
  RegionTemporalIDs(),
  RegionTemporalIDsMutex(),
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
  {
    std::lock_guard<std::mutex> Lock(DataOutMutex);
    if (!DataOut) {
      DataOut = StreamAllocator.getProcessDataStream();
    }
    
    OutputEnabled = true;
  }
  
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
  traceClose();
}


//===----------------------------------------------------------------------===//
// Trace writing control.
//===----------------------------------------------------------------------===//

void TraceProcessListener::traceWriteProcessData() {
  if (!OutputEnabled) {
    return;
  }
  
  auto Out = StreamAllocator.getProcessTraceStream();
  if (!Out) {
    assert(false && "couldn't get stream for process trace.");
    return;
  }
  
  uint64_t const Version = formatVersion();

  // Make these fixed-width for the trace file.
  std::vector<uint64_t> GlobalVariableAddresses64;
  std::vector<uint64_t> FunctionAddresses64;
  std::vector<uint64_t> StreamsInitial64;

  GlobalVariableAddresses64.reserve(GlobalVariableAddresses.size());
  for (auto const Val : GlobalVariableAddresses)
    GlobalVariableAddresses64.push_back(Val);

  FunctionAddresses64.reserve(FunctionAddresses.size());
  for (auto const Val : FunctionAddresses)
    FunctionAddresses64.push_back(Val);

  StreamsInitial64.reserve(StreamsInitial.size());
  for (auto const Val : StreamsInitial)
    StreamsInitial64.push_back(Val);
  
  auto &OutStream = Out->getOStream();
  
  writeBinary(OutStream, Version);
  writeBinary(OutStream, Module.getModuleIdentifier());
  writeBinary(OutStream, GlobalVariableAddresses64);
  writeBinary(OutStream, GlobalVariableInitialData);
  writeBinary(OutStream, FunctionAddresses64);
  writeBinary(OutStream, StreamsInitial64);
  
  auto const BlockOffset = OutputBlockBuilder::flush(std::move(Out));
  assert(BlockOffset && "Couldn't write process trace block?");
}

void TraceProcessListener::traceClose() {
  OutputEnabled = false;
}


//===----------------------------------------------------------------------===//
// Accessors.
//===----------------------------------------------------------------------===//

llvm::Function const *
TraceProcessListener::getFunctionAt(uintptr_t const Address) const
{
  auto const It = FunctionLookup.find(Address);
  return It != FunctionLookup.end() ? It->second : nullptr;
}

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
// Callback when runtime errors are detected.
//===----------------------------------------------------------------------===//

void
TraceProcessListener::setRunErrorCallback(std::function<RunErrorCallbackTy> C)
{
  RunErrorCallback = C;
}

auto TraceProcessListener::getRunErrorCallback() const
-> decltype(RunErrorCallback) const &
{
  return RunErrorCallback;
}


//===----------------------------------------------------------------------===//
// Pointer origin tracking.
//===----------------------------------------------------------------------===//

uint64_t
TraceProcessListener::incrementRegionTemporalID(uintptr_t const Address)
{
  std::lock_guard<std::mutex> Lock(RegionTemporalIDsMutex);
  return ++RegionTemporalIDs[Address];
}

uint64_t
TraceProcessListener::getRegionTemporalID(uintptr_t const Address) const
{
  std::lock_guard<std::mutex> Lock(RegionTemporalIDsMutex);
  auto const It = RegionTemporalIDs.find(Address);
  return It != RegionTemporalIDs.end() ? It->second : 0;
}

PointerTarget
TraceProcessListener::makePointerObject(uintptr_t const ForAddress) const
{
  return PointerTarget(ForAddress, getRegionTemporalID(ForAddress));
}

PointerTarget TraceProcessListener::getPointerObject(llvm::Value const *V) const
{
  // These Values all exist for the lifetime of the program, so we use zero as
  // the temporal identifier.

  if (auto const GV = llvm::dyn_cast<llvm::GlobalVariable>(V)) {
    auto const Address = getRuntimeAddress(GV);
    auto const GlobIt  = GlobalVariableLookup.find(Address);
    assert(GlobIt != GlobalVariableLookup.end());
    return PointerTarget(GlobIt->Begin, 0);
  }
  if (auto const F = llvm::dyn_cast<llvm::Function>(V)) {
    return PointerTarget(getRuntimeAddress(F), 0);
  }
  if (llvm::isa<llvm::ConstantPointerNull>(V))
    return PointerTarget(0, 0);

  llvm::errs() << *V << "\n";
  llvm_unreachable("don't know how to get pointer object.");

  return PointerTarget(0, 0);
}

PointerTarget
TraceProcessListener::getInMemoryPointerObject(uintptr_t const PtrLocation)
const
{
  auto const It = InMemoryPointerObjects.find(PtrLocation);

#if SEEC_DEBUG_IMPO
  if (It == InMemoryPointerObjects.end())
    llvm::errs() << "impo @" << PtrLocation << " not found.\n";
  else
    llvm::errs() << "impo @" << PtrLocation << " = " << It->second << "\n";
#endif

  return It != InMemoryPointerObjects.end() ? It->second
                                            : PointerTarget(0, 0);
}

void TraceProcessListener::setInMemoryPointerObject(uintptr_t const PtrLocation,
                                                    PointerTarget const &Object)
{
  clearInMemoryPointerObjects(MemoryArea(PtrLocation, sizeof(void *)));
  InMemoryPointerObjects[PtrLocation] = Object;
#if SEEC_DEBUG_IMPO
  llvm::errs() << "set impo @" << PtrLocation << " to " << Object << "\n";
#endif
}

void TraceProcessListener::clearInMemoryPointerObjects(MemoryArea const Area)
{
  auto const It = InMemoryPointerObjects.lower_bound(Area.start());
  if (It == InMemoryPointerObjects.end() || It->first >= Area.end())
    return;

  auto const End = InMemoryPointerObjects.lower_bound(Area.end());
#if SEEC_DEBUG_IMPO
  llvm::errs() << "clearing " << std::distance(It, End) << " impos in range ["
               << Area.start() << ", " << Area.end() << ")\n";
#endif
  InMemoryPointerObjects.erase(It, End);
}

void TraceProcessListener::copyInMemoryPointerObjects(uintptr_t const From,
                                                      uintptr_t const To,
                                                      std::size_t const Length)
{
  auto const End = From + Length;
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

void TraceProcessListener::addKnownMemoryRegion(uintptr_t Address,
                                                std::size_t Length,
                                                MemoryPermission Access)
{
  TraceMemory.addAllocation(Address, Length);
  KnownMemory.insert(Address, Address + (Length - 1), Access);
  incrementRegionTemporalID(Address);
}

bool TraceProcessListener::removeKnownMemoryRegion(uintptr_t Address)
{
  TraceMemory.removeAllocation(Address);
  return KnownMemory.erase(Address) != 0;
}


//===----------------------------------------------------------------------===//
// Dynamic memory allocation tracking
//===----------------------------------------------------------------------===//

/// \brief Get the \c DynamicAllocation at the given address.
///
DynamicAllocation const *
TraceProcessListener::getCurrentDynamicMemoryAllocation(uintptr_t const Address)
const
{
  std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);

  auto const It = DynamicMemoryAllocations.find(Address);
  if (It != DynamicMemoryAllocations.end())
    return &(It->second);

  return nullptr;
}

void TraceProcessListener::setCurrentDynamicMemoryAllocation(uintptr_t Address,
                                                             uint32_t Thread,
                                                             offset_uint Offset,
                                                             std::size_t Size)
{
  std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);

  // if the address is already allocated, update its details (realloc)
  auto It = DynamicMemoryAllocations.find(Address);
  if (It != DynamicMemoryAllocations.end()) {
    TraceMemory.resizeAllocation(Address, Size);
    It->second.update(Thread, Offset, Size);
    incrementRegionTemporalID(Address);
  }
  else {
    TraceMemory.addAllocation(Address, Size);
    DynamicMemoryAllocations.insert(
      std::make_pair(Address,
                      DynamicAllocation(Thread, Offset, Address, Size)));
    incrementRegionTemporalID(Address);
  }
}

bool
TraceProcessListener::removeCurrentDynamicMemoryAllocation(uintptr_t Address) {
  std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);
  TraceMemory.removeAllocation(Address);
  return DynamicMemoryAllocations.erase(Address);
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

  auto const Inserted = GlobalVariableLookup.insert(Start, End, GV);
  if (!Inserted.second) {
    if (Inserted.first->Begin > Start && Inserted.first->End < End) {
      GlobalVariableLookup.erase(Inserted.first);
      GlobalVariableLookup.insert(Start, End, GV);
    }
    else if (Inserted.first->Begin > Start || Inserted.first->End < End) {
      llvm_unreachable("overlapping global variables!");
    }
  }
  
  // Create an allocation for this gv, and make it full initialized.
  if (auto const ExistingAlloc = TraceMemory.findAllocationContaining(Start)) {
    // We have to delete the existing allocation and make a new allocation
    // that covers the full region.
    auto const NewStart = std::min(Start, ExistingAlloc->getAddress());
    auto const NewEnd   = std::max(Start + Length,
                                   ExistingAlloc->getArea().end());
    auto const NewLength = NewEnd - NewStart;
    TraceMemory.removeAllocation(ExistingAlloc->getAddress());
    TraceMemory.addAllocation(NewStart, NewLength);
    TraceMemory.add(NewStart, NewLength);
  }
  else {
    TraceMemory.addAllocation(Start, Length);
    TraceMemory.add(Start, Length);
  }
  
  auto Offset = recordData(reinterpret_cast<char const *>(Address), Length);
  GlobalVariableInitialData[Index] = Offset;
}

/// \brief Determine if an \c llvm::Type is a pointer type or contains a pointer
///        type (e.g. is a struct containing a pointer).
///
static bool TypeIsOrContainsPointer(llvm::Type const *Ty)
{
  if (Ty->isPointerTy())
    return true;

  if (auto const STy = llvm::dyn_cast<llvm::StructType>(Ty)) {
    for (auto const &Elem : seec::range(STy->element_begin(),
                                        STy->element_end()))
    {
      if (TypeIsOrContainsPointer(Elem))
        return true;
    }
  }
  else if (auto const STy = llvm::dyn_cast<llvm::SequentialType>(Ty)) {
    return TypeIsOrContainsPointer(STy->getElementType());
  }

  return false;
}

void TraceProcessListener::setGVInitialIMPO(llvm::Type *ElemTy,
                                            uintptr_t Address)
{
  if (llvm::isa<llvm::PointerType>(ElemTy)) {
    // Get the value of the pointer from memory.
    auto const Value = *reinterpret_cast<uintptr_t const *>(Address);
    if (Value) {
      // Find the GlobalVariable that the pointer points to.
      auto const AreaIt = GlobalVariableLookup.find(Value);
      if (AreaIt != GlobalVariableLookup.end())
        setInMemoryPointerObject(Address, PointerTarget(AreaIt->Begin, 0));
    }
  }
  else if (auto const STy = llvm::dyn_cast<llvm::StructType>(ElemTy)) {
    auto const NumElems = STy->getNumElements();
    auto const Layout   = DL.getStructLayout(STy);

    for (unsigned i = 0; i < NumElems; ++i)
      setGVInitialIMPO(STy->getElementType(i),
                       Address + Layout->getElementOffset(i));
  }
  else if (auto const ATy = llvm::dyn_cast<llvm::ArrayType>(ElemTy)) {
    auto const SubElemTy = ATy->getElementType();
    auto const NumElems  = ATy->getNumElements();
    auto const AllocSize = DL.getTypeAllocSize(SubElemTy);

    for (unsigned i = 0; i < NumElems; ++i)
      setGVInitialIMPO(SubElemTy, Address + (i * AllocSize));
  }
  else if (auto const VTy = llvm::dyn_cast<llvm::VectorType>(ElemTy)) {
    auto const SubElemTy = VTy->getElementType();
    auto const NumElems  = VTy->getNumElements();
    auto const AllocSize = DL.getTypeAllocSize(SubElemTy);

    for (unsigned i = 0; i < NumElems; ++i)
      setGVInitialIMPO(SubElemTy, Address + (i * AllocSize));
  }
}

void TraceProcessListener::notifyGlobalVariablesComplete()
{
  // Now we have to iterate over all pointers that are in the memory of global
  // variables and set the relevant in-memory pointer object information.
  for (auto const &GVEntry : GlobalVariableLookup) {
    auto const ElemTy = GVEntry.Value->getType()->getElementType();
    if (!TypeIsOrContainsPointer(ElemTy))
      continue;

    setGVInitialIMPO(ElemTy, GVEntry.Begin);
  }
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
