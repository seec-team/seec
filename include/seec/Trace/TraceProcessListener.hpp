//===- include/seec/Trace/TraceProcessListener.hpp ------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_TRACEPROCESSLISTENER_HPP
#define SEEC_TRACE_TRACEPROCESSLISTENER_HPP

#include "seec/DSA/IntervalMapVector.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/Trace/DetectCallsLookup.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceMemory.hpp"
#include "seec/Trace/TraceStorage.hpp"
#include "seec/Trace/TraceStreams.hpp"
#include "seec/Util/LockedObjectAccessor.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Serialization.hpp"

#include "llvm/DataLayout.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

// #include <atomic>
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

class SynchronizedExit;

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

  typedef llvm::raw_ostream OutStream;
  typedef std::unique_ptr<OutStream> StreamHolder;

  /// Allocates output streams.
  OutputStreamAllocator &StreamAllocator;
  
  /// Handles synchronized calls to std::exit().
  SynchronizedExit &SyncExit;

  /// Original uninstrumented Module.
  llvm::Module &Module;

  /// DataLayout for the Module.
  llvm::DataLayout DL;

  /// Shared index for the uninstrumented Module.
  ModuleIndex &MIndex;

  /// Lookup for detecting calls to C standard library functions.
  seec::trace::detect_calls::Lookup DetectCallsLookup;


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
  std::unique_ptr<llvm::raw_ostream> DataOut;

  /// Number of bytes written to DataOut so far.
  offset_uint DataOutOffset;

  /// Controls access to the DataOut stream.
  std::mutex DataOutMutex;


  /// Synthetic ``process time'' for this process.
  uint64_t Time;


  /// Integer ID given to the next requesting thread.
  uint32_t NextThreadID;
  
  /// Lookup active TraceThreadListener objects.
  llvm::DenseMap<uint32_t, TraceThreadListener const *> ActiveThreadListeners;
  
  /// Controls access to NextThreadID and ActiveThreadListeners.
  mutable std::mutex TraceThreadListenerMutex;


  /// Global memory mutex.
  std::mutex GlobalMemoryMutex;

  /// Controls access to TraceMemory.
  mutable std::mutex TraceMemoryMutex;

  /// Keeps information about the current state of traced memory.
  TraceMemoryState TraceMemory;
  
  /// Keeps information about known, but unowned, areas of memory.
  IntervalMapVector<uintptr_t, MemoryPermission> KnownMemory;


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


public:
  /// \brief Constructor.
  /// \param Module a copy of the original, uninstrumented Module.
  TraceProcessListener(llvm::Module &Module,
                       ModuleIndex &MIndex,
                       OutputStreamAllocator &StreamAllocator,
                       SynchronizedExit &SyncExit);

  /// \brief Destructor.
  ~TraceProcessListener();


  /// \name Accessors
  /// @{
  
  /// \brief Get the shared SynchronizedExit object.
  SynchronizedExit &syncExit() { return SyncExit; }

  /// \brief Get the uninstrumented Module.
  llvm::Module &module() { return Module; }

  /// \brief Get the DataLayout for this Module.
  llvm::DataLayout &dataLayout() { return DL; }

  /// \brief Get the shared module index.
  ModuleIndex &moduleIndex() { return MIndex; }

  /// \brief Get the run-time address of a GlobalVariable.
  /// \param GV the GlobalVariable.
  /// \return the run-time address of GV, or 0 if it is not known.
  uintptr_t getRuntimeAddress(llvm::GlobalVariable const *GV) {
    auto MaybeIndex = MIndex.getIndexOfGlobal(GV);
    if (!MaybeIndex.assigned())
      return 0;

    auto Index = MaybeIndex.get<0>();
    if (Index >= GlobalVariableAddresses.size())
      return 0;

    return GlobalVariableAddresses[Index];
  }

  /// \brief Get the run-time address of a Function.
  /// \param F the Function.
  /// \return the run-time address of F, or 0 if it is not known.
  uintptr_t getRuntimeAddress(llvm::Function const *F) {
    auto MaybeIndex = MIndex.getIndexOfFunction(F);
    if (!MaybeIndex.assigned())
      return 0;

    auto Index = MaybeIndex.get<0>();
    if (Index >= FunctionAddresses.size())
      return 0;

    return FunctionAddresses[Index];
  }
  
  /// \brief Find the allocated range that owns an address.
  /// This method will search dynamic allocations first. If no dynamic
  /// allocation owns the address, then it will search the stack of all
  /// TracingThreadListeners other than that of the thread that requested the
  /// information. This method is thread safe.
  seec::util::Maybe<MemoryArea>
  getContainingMemoryArea(uintptr_t Address, uint32_t RequestingThreadID) const;
  
  /// \brief Get the detect calls Lookup.
  seec::trace::detect_calls::Lookup const &getDetectCallsLookup() const {
    return DetectCallsLookup;
  }

  /// @} (Accessors)
  
  
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
  uint32_t registerThreadListener(TraceThreadListener const *Listener) {
    std::lock_guard<std::mutex> Lock(TraceThreadListenerMutex);
    
    auto ThreadID = NextThreadID++;
    ActiveThreadListeners[ThreadID] = Listener;
    
    return ThreadID;
  }
  
  /// \brief Deregister the TraceThreadListener for ThreadID.
  /// \param ThreadID A ThreadID associated with an active TraceThreadListener.
  void deregisterThreadListener(uint32_t ThreadID) {
    std::lock_guard<std::mutex> Lock(TraceThreadListenerMutex);
    
    ActiveThreadListeners.erase(ThreadID);
  }
  
  /// @}


  /// \name Memory state tracking
  /// @{

  /// \brief Record a block of data, and return the offset of the record.
  offset_uint recordData(char const *Data, size_t Size);

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
                            MemoryPermission Access) {
    KnownMemory.insert(Address, Address + (Length - 1), Access);
  }
  
  /// \brief Remove the region of known memory starting at Address.
  bool removeKnownMemoryRegion(uintptr_t Address) {
    return KnownMemory.erase(Address) != 0;
  }

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

  /// Get information about the Malloc event that allocated an address.
  seec::util::Maybe<DynamicAllocation>
  getCurrentDynamicMemoryAllocation(uintptr_t Address) const {
    std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);

    auto It = DynamicMemoryAllocations.find(Address);
    if (It != DynamicMemoryAllocations.end()) {
      // give the client a copy of the DynamicAllocation
      return It->second;
    }

    return seec::util::Maybe<DynamicAllocation>();
  }

  /// Set the offset of the Malloc event that allocated an address.
  /// \param Address
  /// \param Thread
  /// \param Offset
  void setCurrentDynamicMemoryAllocation(uintptr_t Address,
                                         uint32_t Thread,
                                         offset_uint Offset,
                                         std::size_t Size) {
    std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);

    // if the address is already allocated, update its details (realloc)
    auto It = DynamicMemoryAllocations.find(Address);
    if (It != DynamicMemoryAllocations.end()) {
      It->second.update(Thread, Offset, Size);
    }
    else {
      DynamicMemoryAllocations.insert(
        std::make_pair(Address,
                       DynamicAllocation(Thread, Offset, Address, Size)));
    }
  }

  /// Remove the dynamic memory allocation for an address.
  bool removeCurrentDynamicMemoryAllocation(uintptr_t Address) {
    std::lock_guard<std::mutex> Lock(DynamicMemoryAllocationsMutex);

    return DynamicMemoryAllocations.erase(Address);
  }

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


  /// \name Notifications
  /// @{

  /// \brief Receive the run-time address of a GlobalVariable.
  void notifyGlobalVariable(uint32_t Index,
                            llvm::GlobalVariable const *GV,
                            void const *Addr);

  /// \brief Receive the run-time address of a Function.
  void notifyFunction(uint32_t Index,
                      llvm::Function const *F,
                      void const *Addr);

  /// @} (Notifications)
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEPROCESSLISTENER_HPP
