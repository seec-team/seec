//===- include/seec/Trace/ProcessState.hpp -------------------------- C++ -===//
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

#ifndef SEEC_TRACE_PROCESSSTATE_HPP
#define SEEC_TRACE_PROCESSSTATE_HPP

#include "seec/DSA/IntervalMapVector.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/StreamState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <thread>

namespace llvm {
  class raw_ostream; // Forward-declaration for operator<<.
}

namespace seec {

class ModuleIndex;

namespace trace {


/// \brief State of a single dynamic memory allocation.
///
class MallocState {
  /// Address of the allocated memory.
  uintptr_t Address;

  /// Size of the allocated memory.
  std::size_t Size;

  /// Allocator \c llvm::Instruction pointers. The most recent is the "current"
  /// allocator (responsible for the most recent allocation).
  llvm::SmallVector<llvm::Instruction const *, 1> Allocators;

public:
  /// \brief Construct a new \c MallocState with the given values.
  ///
  MallocState(uintptr_t Address,
              std::size_t Size,
              llvm::Instruction const *WithAllocator)
  : Address(Address),
    Size(Size),
    Allocators(1, WithAllocator)
  {}

  /// \brief Get the address of the allocated memory.
  ///
  uintptr_t getAddress() const { return Address; }

  /// \brief Get the size of the allocated memory.
  ///
  std::size_t getSize() const { return Size; }

  /// \brief Get the \c llvm::Instruction that caused this allocation.
  ///
  llvm::Instruction const *getAllocator() const {
    return Allocators.back();
  }

  /// \brief Add a new allocator \c llvm::Instruction (for realloc).
  ///
  void pushAllocator(llvm::Instruction const *I) {
    Allocators.push_back(I);
  }

  /// \brief Rewind to the previous allocator \c llvm::Instruction
  ///        (for realloc).
  ///
  void popAllocator() {
    Allocators.pop_back();
    assert(!Allocators.empty());
  }

  /// \brief Set the size of the allocated memory (for realloc).
  ///
  void setSize(std::size_t const Value) { Size = Value; }
};


/// \brief State of a process at a specific point in time.
///
class ProcessState {
  friend class ThreadState; // Allow child threads to update the shared state.

  /// \name Constants
  /// @{

  /// The ProcessTrace that this state was produced from.
  std::shared_ptr<ProcessTrace const> Trace;

  /// Indexed view of the llvm::Module that this trace was created from.
  std::shared_ptr<ModuleIndex const> Module;
  
  /// DataLayout for the llvm::Module that this trace was created from.
  llvm::DataLayout DL;

  /// @} (Constants.)


private:
  /// \name Variable data
  /// @{

  /// The synthetic process time that this state represents.
  std::atomic<uint64_t> ProcessTime;

  /// Thread states, indexed by (ThreadID - 1).
  std::vector<std::unique_ptr<ThreadState>> ThreadStates;

  /// All current dynamic memory allocations, indexed by address.
  std::map<uintptr_t, MallocState> Mallocs;

  /// Previous dynamic memory allocations, from oldest to youngest.
  std::vector<MallocState> PreviousMallocs;

  /// Current state of memory.
  MemoryState Memory;
  
  /// Known, but unowned, regions of memory.
  IntervalMapVector<uintptr_t, MemoryPermission> KnownMemory;
  
  /// Currently open streams.
  llvm::DenseMap<uintptr_t, StreamState> Streams;

  /// Previously closed streams (in order of closing).
  std::vector<StreamState> StreamsClosed;
  
  /// Currently open DIRs.
  llvm::DenseMap<uintptr_t, DIRState> Dirs;

  /// @} (Variable data.)


  // Don't allow copying.
  ProcessState(ProcessState const &Other) = delete;
  ProcessState &operator=(ProcessState const &RHS) = delete;

public:
  /// \brief Construct a new ProcessState at the beginning of the Trace.
  ///
  ProcessState(std::shared_ptr<ProcessTrace const> Trace,
               std::shared_ptr<ModuleIndex const> ModIndex);
  
  /// \brief Destructor.
  ///
  ~ProcessState();


  /// \name Accessors.
  /// @{

  /// \brief Get the ProcessTrace backing this state.
  ///
  ProcessTrace const &getTrace() const { return *Trace; }

  /// \brief Get a ModuleIndex for the llvm::Module.
  ///
  ModuleIndex const &getModule() const { return *Module; }
  
  /// \brief Get the DataLayout for the llvm::Module.
  ///
  llvm::DataLayout const &getDataLayout() const { return DL; }

  /// \brief Get the synthetic process time that this state represents.
  ///
  uint64_t getProcessTime() const { return ProcessTime; }
  
  /// \brief Get the vector of thread states.
  ///
  decltype(ThreadStates) const &getThreadStates() const { return ThreadStates; }
  
  /// \brief Get the number of thread states.
  ///
  std::size_t getThreadStateCount() const { return ThreadStates.size(); }

  /// \brief Get the thread state for the given Thread ID.
  ///
  ThreadState &getThreadState(uint32_t ThreadID) {
    assert(ThreadID > 0 && ThreadID <= ThreadStates.size());
    return *(ThreadStates[ThreadID - 1]);
  }

  /// \brief Get the thread state for the given Thread ID.
  ///
  ThreadState const &getThreadState(uint32_t ThreadID) const {
    assert(ThreadID > 0 && ThreadID <= ThreadStates.size());
    return *(ThreadStates[ThreadID - 1]);
  }

  /// @} (Accessors.)
  
  
  /// \name Memory.
  /// @{

  /// \brief Add a dynamic memory allocation (moving forwards).
  ///
  void addMalloc(uintptr_t const Address,
                 std::size_t const Size,
                 llvm::Instruction const *Allocator);

  /// \brief Unadd a dynamic memory allocation (moving backwards).
  ///
  void unaddMalloc(uintptr_t const Address);

  /// \brief Remove a dynamic memory allocation (moving forwards).
  ///
  void removeMalloc(uintptr_t const Address);

  /// \brief Unremove a dynamic memory allocation (moving backwards).
  ///
  void unremoveMalloc(uintptr_t const Address);

  /// \brief Get the map of dynamic memory allocations.
  ///
  decltype(Mallocs) const &getMallocs() const { return Mallocs; }

  /// \brief Get the memory state.
  ///
  decltype(Memory) &getMemory() { return Memory; }
  
  /// \brief Get the memory state.
  ///
  decltype(Memory) const &getMemory() const { return Memory; }
  
  /// \brief Add a region of known memory.
  ///
  void addKnownMemory(uintptr_t Address,
                      std::size_t Length,
                      MemoryPermission Access)
  {
    KnownMemory.insert(Address, Address + (Length - 1), Access);
  }
  
  /// \brief Remove the region of known memory at the given address.
  ///
  bool removeKnownMemory(uintptr_t Address) {
    return (KnownMemory.erase(Address) != 0);
  }
  
  /// \brief Get the regions of known memory.
  ///
  decltype(KnownMemory) const &getKnownMemory() const { return KnownMemory; }
  
  /// \brief Find the allocated range that owns an address.
  ///
  /// This method will search in the following order:
  ///  - Global variables.
  ///  - Dynamic memory allocations.
  ///  - Known readable/writable regions.
  ///  - Thread stacks.
  ///
  seec::Maybe<MemoryArea>
  getContainingMemoryArea(uintptr_t Address) const;
  
  /// @} (Memory.)
  
  
  /// \name Streams.
  /// @{
  
  /// \brief Get the currently open streams.
  ///
  decltype(Streams) const &getStreams() const { return Streams; }
  
  /// \brief Add a stream to the currently open streams.
  /// \return true iff the stream was added (did not already exist).
  ///
  bool addStream(StreamState Stream);

  /// \brief Remove a stream from the currently open streams.
  /// \return true iff the stream was removed (existed).
  ///
  bool removeStream(uintptr_t Address);

  /// \brief Close a currently open stream.
  /// \return true iff the stream was closed (existed).
  ///
  bool closeStream(uintptr_t const Address);

  /// \brief Restore the most recently closed stream.
  /// \param Address used to ensure that the correct stream is restored.
  /// \return true iff the stream was restored.
  ///
  bool restoreStream(uintptr_t const Address);
  
  /// \brief Get a pointer to the stream at Address, or nullptr if none exists.
  ///
  StreamState *getStream(uintptr_t Address);
  
  /// \brief Get a pointer to the stream at Address, or nullptr if none exists.
  ///
  StreamState const *getStream(uintptr_t Address) const;
  
  /// \brief Get a pointer to the stdout stream, if it is open.
  ///
  StreamState const *getStreamStdout() const;
  
  /// @} (Streams.)
  
  
  /// \name Dirs.
  /// @{
  
  /// \brief Get the currently open DIRs.
  ///
  decltype(Dirs) const &getDirs() const { return Dirs; }
  
  /// \brief Add a DIR to the currently open DIRs.
  /// \return true iff the DIR was added (did not already exist).
  ///
  bool addDir(DIRState Dir);
  
  /// \brief Remove a DIR from the currently open DIRs.
  /// \return true iff the DIR was removed (existed).
  ///
  bool removeDir(uintptr_t Address);
  
  /// \brief Get a pointer to the DIR at Address, or nullptr if none exists.
  ///
  DIRState const *getDir(uintptr_t Address) const;
  
  /// @} (Dirs.)
  
  
  /// \name Get run-time addresses.
  /// @{
  
  /// \brief Get the run-time address of a Function.
  ///
  uintptr_t getRuntimeAddress(llvm::Function const *F) const;
  
  /// \brief Get the run-time address of a GlobalVariable.
  ///
  uintptr_t getRuntimeAddress(llvm::GlobalVariable const *GV) const;
  
  /// @} (Get run-time addresses.)
};

/// Print a textual description of a ProcessState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_PROCESSSTATE_HPP
