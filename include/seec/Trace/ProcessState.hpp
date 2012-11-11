//===- include/seec/Trace/ProcessState.hpp -------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_PROCESSSTATE_HPP
#define SEEC_TRACE_PROCESSSTATE_HPP

#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceReader.hpp"

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/DataLayout.h"

#include <condition_variable>
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

  /// Location of the Malloc event.
  EventLocation Malloc;

public:
  /// Construct a new MallocState with the given values.
  MallocState(uintptr_t Address,
              std::size_t Size,
              EventLocation MallocLocation)
  : Address(Address),
    Size(Size),
    Malloc(MallocLocation)
  {}

  /// Get the address of the allocated memory.
  uintptr_t getAddress() const { return Address; }

  /// Get the size of the allocated memory.
  std::size_t getSize() const { return Size; }

  /// Get the location of the Malloc event.
  EventLocation getMallocLocation() const { return Malloc; }
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

  /// @}


  /// \name Update access control
  /// @{

  /// Controls access to mutable areas of the process state during updates.
  std::mutex UpdateMutex;

  /// Used to wait for an appropriate update time (i.e. correct ProcessTime).
  std::condition_variable UpdateCV;

  /// \brief Managed update access during its lifetime.
  /// On construction, lock UpdateMutex and ensure that ProcessTime matches
  /// the required time (or wait until it does). On destruction, unlock the
  /// UpdateMutex and notify UpdateCV.
  class ScopedUpdate {
    ProcessState &State;

    std::unique_lock<std::mutex> UpdateLock;

    void unlock() {
      if (UpdateLock) {
        UpdateLock.unlock();
        State.UpdateCV.notify_all();
      }
    }

    // Don't allow copying.
    ScopedUpdate(ScopedUpdate const &Other) = delete;
    ScopedUpdate &operator=(ScopedUpdate const &RHS) = delete;

  public:
    /// \brief Acquire shared update permissions from the given State, when its
    /// ProcessTime is equal to the RequiredProcessTime.
    /// \param State the ProcessState to acquire shared update permissions for.
    /// \param RequiredProcessTime this constructor will block until it has
    ///        update permissions and State.ProcessTime == RequiredProcessTime.
    ScopedUpdate(ProcessState &State, uint64_t RequiredProcessTime)
    : State(State),
      UpdateLock(State.UpdateMutex)
    {
      State.UpdateCV.wait(UpdateLock,
                          [=,&State](){
                            return State.ProcessTime == RequiredProcessTime;
                          });
    }

    /// Move constructor.
    ScopedUpdate(ScopedUpdate &&Other) = default;

    /// Move update permissions from another ScopedUpdate object to this one.
    /// Both ScopedUpdated objects must have been created from the same
    /// ProcessState.
    ScopedUpdate &operator=(ScopedUpdate &&RHS) {
      assert(&State == &(RHS.State)
             && "Can't move update from different ProcessState");
      unlock();
      UpdateLock = std::move(RHS.UpdateLock);
      return *this;
    }

    /// If this ScopedUpdate has a lock on the UpdateMutex, then the destructor
    /// will unlock the UpdateMutex and wake up all ScopedUpdate objects that
    /// are waiting for the UpdateCV.
    ~ScopedUpdate() {
      unlock();
    }
  };

  /// \brief Acquires permission to update the shared state.
  /// The returned object holds permission to update the shared state of this
  /// ProcessState, and releases that permission when it is destructed. The
  /// creation of this object will block until this ProcessState's ProcessTime
  /// is equal to RequiredProcessTime.
  ScopedUpdate getScopedUpdate(uint64_t RequiredProcessTime) {
    return ScopedUpdate(*this, RequiredProcessTime);
  }

  /// @}

  /// \name Variable data
  /// @{

  /// The synthetic process time that this state represents.
  uint64_t ProcessTime;

  /// Thread states, indexed by (ThreadID - 1).
  std::vector<std::unique_ptr<ThreadState>> ThreadStates;

  /// All current dynamic memory allocations, indexed by address.
  llvm::DenseMap<uintptr_t, MallocState> Mallocs;

  /// Current state of memory.
  MemoryState Memory;

  /// @}


  // Don't allow copying.
  ProcessState(ProcessState const &Other) = delete;
  ProcessState &operator=(ProcessState const &RHS) = delete;

public:
  /// Construct a new ProcessState at the beginning of the Trace.
  ProcessState(std::shared_ptr<ProcessTrace const> Trace,
               std::shared_ptr<ModuleIndex const> ModIndex);

  /// \name Accessors.
  /// @{

  /// \brief Get the ProcessTrace backing this state.
  ProcessTrace const &getTrace() const { return *Trace; }

  /// \brief Get a ModuleIndex for the llvm::Module.
  ModuleIndex const &getModule() const { return *Module; }
  
  /// \brief Get the DataLayout for the llvm::Module.
  llvm::DataLayout const &getDataLayout() const { return DL; }

  /// \brief Get the synthetic process time that this state represents.
  uint64_t getProcessTime() const { return ProcessTime; }
  
  /// \brief Get the vector of thread states.
  decltype(ThreadStates) const &getThreadStates() const { return ThreadStates; }
  
  /// \brief Get the number of thread states.
  std::size_t getThreadStateCount() const { return ThreadStates.size(); }

  /// \brief Get the thread state for the given Thread ID.
  ThreadState &getThreadState(uint32_t ThreadID) {
    assert(ThreadID > 0 && ThreadID <= ThreadStates.size());
    return *(ThreadStates[ThreadID - 1]);
  }

  /// \brief Get the thread state for the given Thread ID.
  ThreadState const &getThreadState(uint32_t ThreadID) const {
    assert(ThreadID > 0 && ThreadID <= ThreadStates.size());
    return *(ThreadStates[ThreadID - 1]);
  }

  /// \brief Get the map of dynamic memory allocations.
  decltype(Mallocs) &getMallocs() { return Mallocs; }
  
  /// \brief Get the map of dynamic memory allocations.
  decltype(Mallocs) const &getMallocs() const { return Mallocs; }

  /// \brief Get the memory state.
  decltype(Memory) &getMemory() { return Memory; }
  
  /// \brief Get the memory state.
  decltype(Memory) const &getMemory() const { return Memory; }

  /// @}


  /// \name Mutators.
  /// @{

  /// \brief Set a new ProcessTime and update all states accordingly.
  void setProcessTime(uint64_t Time);

  /// \brief Move to the next ProcessTime (if there is one in this trace).
  ProcessState &operator++();

  /// \brief Move to the previous ProcessTime (if there is one in this trace).
  ProcessState &operator--();

  ///


  /// @}
};

/// Print a textual description of a ProcessState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_PROCESSSTATE_HPP
