//===- include/seec/Trace/TraceProcessListener.hpp ------------------ C++ -===//
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

#ifndef SEEC_TRACE_TRACEPROCESSLISTENER_HPP
#define SEEC_TRACE_TRACEPROCESSLISTENER_HPP

#include "seec/DSA/IntervalMapVector.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/Trace/DetectCallsLookup.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceMemory.hpp"
#include "seec/Trace/TracePointer.hpp"
#include "seec/Trace/TraceStorage.hpp"
#include "seec/Trace/TraceStreams.hpp"
#include "seec/Util/LockedObjectAccessor.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Serialization.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace llvm {
  class Module;
  class GlobalVariable;
  class Function;
} // namespace llvm

namespace seec {

namespace runtime_errors {
  class RunError;
}

namespace trace {
  
class TraceThreadListener;


/// \brief Information about a dynamically allocated memory area.
class DynamicAllocation {
  /// ID of the thread that caused this allocation.
  uint32_t Thread;

  /// Offset of the Malloc event in the thread's event data.
  offset_uint Offset;
  
  /// Address of the allocation.
  uintptr_t Address;

  /// Size of the allocation.
  std::size_t Size;

public:
  DynamicAllocation(uint32_t Thread,
                    offset_uint Offset,
                    uintptr_t Address,
                    std::size_t Size)
  : Thread(Thread),
    Offset(Offset),
    Address(Address),
    Size(Size)
  {}

  DynamicAllocation(DynamicAllocation const &) = default;

  DynamicAllocation &operator=(DynamicAllocation const &) = default;
  
  
  /// \name Accessors
  /// @{

  uint32_t thread() const { return Thread; }

  offset_uint offset() const { return Offset; }
  
  uintptr_t address() const { return Address; }

  std::size_t size() const { return Size; }
  
  MemoryArea area() const { return MemoryArea(Address, Size); }
  
  /// @} (Accessors)
  
  
  /// \name Mutators
  /// @{
  
  void update(uint32_t NewThread, offset_uint NewOffset, std::size_t NewSize) {
    Thread = NewThread;
    Offset = NewOffset;
    Size = NewSize;
  }
  
  /// @} (Mutators)
};


/// \brief Receive and trace process-level events.
class TraceProcessListener {
  // don't allow copying
  TraceProcessListener(TraceProcessListener const &) = delete;
  TraceProcessListener &operator=(TraceProcessListener const &) = delete;

  /// Allocates output streams.
  OutputStreamAllocator &StreamAllocator;
  
  /// Toggles trace output.
  std::atomic_bool OutputEnabled;

  /// Original uninstrumented Module.
  llvm::Module &Module;

  /// DataLayout for the Module.
  llvm::DataLayout DL;

  /// Shared index for the uninstrumented Module.
  ModuleIndex &MIndex;

  /// Lookup for detecting calls to C standard library functions.
  seec::trace::detect_calls::Lookup DetectCallsLookup;

  typedef void RunErrorCallbackTy(seec::runtime_errors::RunError const &,
                                  llvm::Instruction const *);

  /// Callback when runtime errors are detected.
  std::function<RunErrorCallbackTy> RunErrorCallback;


  /// Lookup GlobalVariable's run-time addresses by index.
  std::vector<uintptr_t> GlobalVariableAddresses;

  /// Find GlobalVariables by their run-time addresses.
  IntervalMapVector<uintptr_t, llvm::GlobalVariable const *>
    GlobalVariableLookup;
    
  /// Offsets of data for the initial state of GlobalVariables.
  std::vector<offset_uint> GlobalVariableInitialData;


  /// Lookup Function's run-time addresses by index.
  std::vector<uintptr_t> FunctionAddresses;

  /// Find Functions by their run-time addresses.
  llvm::DenseMap<uintptr_t, llvm::Function const *> FunctionLookup;


  /// Output stream for this process' data.
  std::unique_ptr<OutputBlockProcessDataStream> DataOut;

  /// Controls access to the DataOut stream.
  std::mutex DataOutMutex;


  /// Synthetic ``process time'' for this process.
  uint64_t Time;


  /// Integer ID given to the next requesting thread.
  std::atomic<uint32_t> NextThreadID;
  
  /// Number of active threads.
  std::atomic<int> ActiveThreadCount;
  
  
  /// Synchronize setup of the environ table.
  std::once_flag EnvironSetupOnceFlag;


  /// Global memory mutex.
  std::mutex GlobalMemoryMutex;

  /// Controls access to TraceMemory.
  mutable std::mutex TraceMemoryMutex;

  /// Keeps information about the current state of traced memory.
  TraceMemoryState TraceMemory;
  
  /// Keeps information about known, but unowned, areas of memory.
  IntervalMapVector<uintptr_t, MemoryPermission> KnownMemory;


  /// Temporal identifiers for pointer regions.
  llvm::DenseMap<uintptr_t, uint64_t> RegionTemporalIDs;

  /// Control access to \c RegionTemporalIDs.
  mutable std::mutex RegionTemporalIDsMutex;

  /// Pointer objects.
  std::map<uintptr_t, PointerTarget> InMemoryPointerObjects;


  /// Dynamic memory mutex.
  std::mutex DynamicMemoryMutex;

  /// Lookup for current dynamic memory allocations, by address.
  std::map<uintptr_t, DynamicAllocation> DynamicMemoryAllocations;

  /// Controls internal access to DynamicMemoryAllocations.
  mutable std::mutex DynamicMemoryAllocationsMutex;
  
  
  /// I/O stream mutex.
  mutable std::mutex StreamsMutex;
  
  /// I/O stream information.
  TraceStreams Streams;
  
  /// Runtime addresses of the initial standard input/output streams.
  std::vector<uintptr_t> StreamsInitial;
  
  
  /// DIR tracking mutex.
  mutable std::mutex DirsMutex;
  
  /// DIR tracking information.
  TraceDirs Dirs;


public:
  /// \brief Constructor.
  /// \param Module a copy of the original, uninstrumented Module.
  TraceProcessListener(llvm::Module &Module,
                       ModuleIndex &MIndex,
                       OutputStreamAllocator &StreamAllocator);

  /// \brief Destructor.
  ~TraceProcessListener();
  
  
  /// \name Trace writing control.
  /// @{
  
  /// \brief Check if tracing is enabled.
  ///
  bool traceEnabled() const { return OutputEnabled; }
  
  /// \brief Write out trace information (function addresses, etc.).
  ///
  void traceWriteProcessData();
  
  /// \brief Close all open trace streams and disable future writes.
  ///
  void traceClose();
  
  /// @}


  /// \name Accessors
  /// @{

  /// \brief Get the uninstrumented Module.
  llvm::Module &module() const { return Module; }

  /// \brief Get the DataLayout for this Module.
  llvm::DataLayout const &getDataLayout() const { return DL; }

  /// \brief Get the shared module index.
  ModuleIndex const &moduleIndex() const { return MIndex; }

  /// \brief Get the run-time address of a GlobalVariable.
  /// \param GV the GlobalVariable.
  /// \return the run-time address of GV, or 0 if it is not known.
  uintptr_t getRuntimeAddress(llvm::GlobalVariable const *GV) const {
    auto MaybeIndex = MIndex.getIndexOfGlobal(GV);
    if (!MaybeIndex)
      return 0;

    if (*MaybeIndex >= GlobalVariableAddresses.size())
      return 0;

    return GlobalVariableAddresses[*MaybeIndex];
  }

  /// \brief Get the run-time address of a Function.
  /// \param F the Function.
  /// \return the run-time address of F, or 0 if it is not known.
  uintptr_t getRuntimeAddress(llvm::Function const *F) const {
    auto MaybeIndex = MIndex.getIndexOfFunction(F);
    if (!MaybeIndex)
      return 0;

    if (*MaybeIndex >= FunctionAddresses.size())
      return 0;

    return FunctionAddresses[*MaybeIndex];
  }

  /// \brief Find the \c llvm::Function at the given address.
  /// \param Address the runtime address.
  /// \return the \c llvm::Function at \c Address, or \c nullptr if none known.
  ///
  llvm::Function const *getFunctionAt(uintptr_t const Address) const;
  
  /// \brief Find the allocated range that owns an address.
  /// Requires access to TraceMemoryState.
  ///
  seec::Maybe<MemoryArea> getContainingMemoryArea(uintptr_t Address) const;
  
  /// \brief Get the detect calls Lookup.
  seec::trace::detect_calls::Lookup const &getDetectCallsLookup() const {
    return DetectCallsLookup;
  }
  
  std::once_flag &getEnvironSetupOnceFlag() {
    return EnvironSetupOnceFlag;
  }

  /// @} (Accessors)


  /// \name Callback when runtime errors are detected.
  /// @{

  void setRunErrorCallback(std::function<RunErrorCallbackTy> Callback);

  decltype(RunErrorCallback) const &getRunErrorCallback() const;

  /// @}
  
  
  /// \name Synthetic process time
  /// @{
  
  /// \brief Get the current process time.
  uint64_t getTime() const {
    return Time;
  }
  
  /// \brief Increment the process time and get the new value.
  uint64_t getNewTime() {
    return ++Time;
  }
  
  /// @}
  
  
  /// \name TraceThreadListener registration
  /// @{
  
  /// \brief Register a new TraceThreadListener with this process.
  /// \return a new integer ThreadID for the TraceThreadListener.
  uint32_t registerThreadListener(TraceThreadListener *Listener) {
    ActiveThreadCount++;
    return NextThreadID++;
  }
  
  /// \brief Deregister the TraceThreadListener for ThreadID.
  /// \param ThreadID A ThreadID associated with an active TraceThreadListener.
  void deregisterThreadListener(uint32_t ThreadID) {
    ActiveThreadCount--;
  }
  
  /// \brief Get the number of active TraceThreadListener objects.
  ///
  int countThreadListeners() const {
    return ActiveThreadCount;
  }
  
  /// @}


  /// \name Pointer object tracking
  /// @{

  /// \brief Increment the temporal ID for the region starting at \c Address.
  ///
  uint64_t incrementRegionTemporalID(uintptr_t const Address);

  /// \brief Get the temporal ID for the region starting at \c Address.
  ///
  uint64_t getRegionTemporalID(uintptr_t const Address) const;

  /// \brief Get a current \c PointerTarget for the region starting at
  ///        \c Address.
  ///
  PointerTarget makePointerObject(uintptr_t const ForAddress) const;

  /// \brief Get the \c PointerTarget for the given \c llvm::Value.
  ///
  PointerTarget getPointerObject(llvm::Value const *V) const;

  /// \brief Get the \c PointerTarget for the pointer that is in-memory
  ///        starting at address \c PtrLocation.
  ///
  PointerTarget getInMemoryPointerObject(uintptr_t const PtrLocation) const;

  void setInMemoryPointerObject(uintptr_t const PtrLocation,
                                PointerTarget const &Object);

  void clearInMemoryPointerObjects(MemoryArea const Area);

  void copyInMemoryPointerObjects(uintptr_t const From,
                                  uintptr_t const To,
                                  std::size_t const Length);

  /// @} (Pointer object tracking)


  /// \name Memory state tracking
  /// @{

  /// \brief Record a block of data, and return the offset of the record.
  llvm::Optional<off_t> recordData(char const *Data, size_t Size);

  /// \brief Lock a region of memory.
  std::unique_lock<std::mutex> lockMemory() {
    return std::unique_lock<std::mutex>(GlobalMemoryMutex);
  }
  
  /// \brief Get access to this ProcessListener's TraceMemoryState.
  LockedObjectAccessor<TraceMemoryState, std::mutex>
  getTraceMemoryStateAccessor() {
    return makeLockedObjectAccessor(TraceMemoryMutex, TraceMemory);
  }
  
  /// \brief Get const access to this ProcessListener's TraceMemoryState.
  LockedObjectAccessor<TraceMemoryState const, std::mutex>
  getTraceMemoryStateAccessor() const {
    return makeLockedObjectAccessor(TraceMemoryMutex, TraceMemory);
  }
  
  /// \brief Add a region of known, but unowned, memory.
  void addKnownMemoryRegion(uintptr_t Address,
                            std::size_t Length,
                            MemoryPermission Access);
  
  /// \brief Remove the region of known memory starting at Address.
  bool removeKnownMemoryRegion(uintptr_t Address);
  
  /// \brief Get const access to the known memory regions.
  ///
  decltype(KnownMemory) const &getKnownMemory() const { return KnownMemory; }

  /// @}


  /// \name Dynamic memory allocation tracking
  /// @{

  /// \brief Acquire dynamic memory lock.
  /// Used to prevent race conditions with dynamic memory handling.
  std::unique_lock<std::mutex> lockDynamicMemory() {
    return std::unique_lock<std::mutex>(DynamicMemoryMutex);
  }

  /// Check if an address is the start of a dynamically allocated memory block.
  bool isCurrentDynamicMemoryAllocation(uintptr_t Address) const {
    std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);

    return DynamicMemoryAllocations.count(Address);
  }

  /// \brief Get the \c DynamicAllocation at the given address.
  ///
  DynamicAllocation const *
  getCurrentDynamicMemoryAllocation(uintptr_t const Address) const;

  /// Set the offset of the Malloc event that allocated an address.
  /// \param Address
  /// \param Thread
  /// \param Offset
  void setCurrentDynamicMemoryAllocation(uintptr_t Address,
                                         uint32_t Thread,
                                         offset_uint Offset,
                                         std::size_t Size);

  /// Remove the dynamic memory allocation for an address.
  bool removeCurrentDynamicMemoryAllocation(uintptr_t Address);

  /// @} (Dynamic memory allocation tracking)
  
  
  /// \name I/O streams tracking.
  /// @{
  
  /// \brief Lock the I/O streams.
  std::unique_lock<std::mutex> getStreamsLock() const {
    return std::unique_lock<std::mutex>(StreamsMutex);
  }
  
  /// \brief Get an accessor to the streams information.
  LockedObjectAccessor<TraceStreams, std::mutex>
  getStreamsAccessor() {
    return makeLockedObjectAccessor(StreamsMutex, Streams);
  }
  
  /// \brief Get an accessor to the streams information.
  LockedObjectAccessor<TraceStreams const, std::mutex>
  getStreamsAccessor() const {
    return makeLockedObjectAccessor(StreamsMutex, Streams);
  }
  
  /// \brief Get the streams information.
  TraceStreams &
  getStreams(std::unique_lock<std::mutex> const &Lock) {
    assert(Lock.mutex() == &StreamsMutex && Lock.owns_lock());
    return Streams;
  }
  
  /// \brief Get the streams information.
  TraceStreams const &
  getStreams(std::unique_lock<std::mutex> const &Lock) const {
    assert(Lock.mutex() == &StreamsMutex && Lock.owns_lock());
    return Streams;
  }
  
  /// @} (I/O streams tracking)
  
  
  /// \name DIR tracking.
  /// @{
  
  /// \brief Lock the DIR tracking.
  std::unique_lock<std::mutex> getDirsLock() const {
    return std::unique_lock<std::mutex>(DirsMutex);
  }
  
  /// \brief Get an accessor to the DIR tracking information.
  LockedObjectAccessor<TraceDirs, std::mutex>
  getDirsAccessor() {
    return makeLockedObjectAccessor(DirsMutex, Dirs);
  }
  
  /// \brief Get an accessor to the DIR tracking information.
  LockedObjectAccessor<TraceDirs const, std::mutex>
  getDirsAccessor() const {
    return makeLockedObjectAccessor(DirsMutex, Dirs);
  }
  
  /// \brief Get the DIR tracking information.
  TraceDirs &
  getDirs(std::unique_lock<std::mutex> const &Lock) {
    assert(Lock.mutex() == &DirsMutex && Lock.owns_lock());
    return Dirs;
  }
  
  /// \brief Get the DIR tracking information.
  TraceDirs const &
  getDirs(std::unique_lock<std::mutex> const &Lock) const {
    assert(Lock.mutex() == &DirsMutex && Lock.owns_lock());
    return Dirs;
  }
  
  /// @} (I/O streams tracking)


  /// \name Notifications
  /// @{

  /// \brief Receive the run-time address of a GlobalVariable.
  void notifyGlobalVariable(uint32_t Index,
                            llvm::GlobalVariable const *GV,
                            void const *Addr);

private:
  ///
  void setGVInitialIMPO(llvm::Type *ElemTy, uintptr_t Address);

public:
  /// \brief Called when all GlobalVariable run-time addresses received.
  void notifyGlobalVariablesComplete();

  /// \brief Receive the run-time address of a Function.
  void notifyFunction(uint32_t Index,
                      llvm::Function const *F,
                      void const *Addr);

  /// @} (Notifications)
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEPROCESSLISTENER_HPP
