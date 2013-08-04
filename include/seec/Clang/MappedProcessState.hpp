//===- include/seec/Clang/MappedProcessState.hpp --------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines ProcessState, a class for viewing the recorded states of
/// SeeC-Clang mapped processes. 
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MAPPEDPROCESSSTATE_HPP
#define SEEC_CLANG_MAPPEDPROCESSSTATE_HPP

#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedStreamState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/DSA/MemoryArea.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/DenseMap.h"

#include <memory>
#include <vector>


namespace clang {
  class Decl;
} // namespace clang

namespace llvm {
  class raw_ostream;
  class GlobalVariable;
} // namespace llvm


namespace seec {

namespace trace {
  class ProcessState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class GlobalVariable;
class ProcessTrace;
class ThreadState;


/// \brief SeeC-Clang-mapped process state.
///
class ProcessState {
  /// The SeeC-Clang-mapped trace.
  seec::cm::ProcessTrace const &Trace;
  
  /// The base (unmapped) state.
  std::unique_ptr<seec::trace::ProcessState> UnmappedState;
  
  /// Global variables.
  std::vector<std::unique_ptr<seec::cm::GlobalVariable>> GlobalVariableStates;
  
  /// Unmapped global variables.
  std::vector<seec::MemoryArea> UnmappedStaticAreas;
  
  /// Thread states.
  std::vector<std::unique_ptr<seec::cm::ThreadState>> ThreadStates;
  
  /// The current Value store.
  std::shared_ptr<ValueStore const> CurrentValueStore;
  
  /// Currently open streams.
  llvm::DenseMap<uintptr_t, StreamState> Streams;
  
public:
  /// \brief Constructor.
  ///
  ProcessState(seec::cm::ProcessTrace const &Trace);
  
  /// \brief Move constructor.
  ///
  ProcessState(ProcessState &&) = default;
  
  // No copy constructor.
  ProcessState(ProcessState const &Other) = delete;
  
  /// \brief Move assignment.
  ///
  ProcessState &operator=(ProcessState &&) = default;
  
  // No copy assignment.
  ProcessState &operator=(ProcessState const &RHS) = delete;
  
  /// \brief Destructor.
  ///
  ~ProcessState();
  
  
  /// \brief Clear any cached information.
  ///
  /// Must be called following updates to the underlying state. May eagerly
  /// generate new information.
  ///
  void cacheClear();
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation) const;
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the unmapped process state for this state.
  ///
  seec::trace::ProcessState &getUnmappedProcessState() {
    return *UnmappedState;
  }
  
  /// \brief Get the unmapped process state for this state.
  ///
  seec::trace::ProcessState const &getUnmappedProcessState() const {
    return *UnmappedState;
  }
  
  /// \brief Get the value store used by this state.
  ///
  std::shared_ptr<ValueStore const> getCurrentValueStore() const {
    return CurrentValueStore;
  }
  
  /// \brief Get the synthetic process time for this state.
  ///
  uint64_t getProcessTime() const;
  
  /// @} (Access underlying information).
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the process trace.
  ///
  seec::cm::ProcessTrace const &getProcessTrace() const { return Trace; }
  
  /// @} (Accessors.)
  
  
  /// \name Threads.
  /// @{
  
  /// \brief Get the number of threads.
  std::size_t getThreadCount() const;
  
  /// \brief Get the state of a thread.
  seec::cm::ThreadState &getThread(std::size_t Index);
  
  /// \brief Get the state of a thread.
  seec::cm::ThreadState const &getThread(std::size_t Index) const;
  
  /// @} (Threads.)
  
  
  /// \name Global variables.
  /// @{
  
  /// \brief Get all mapped global variables.
  ///
  decltype(GlobalVariableStates) const &getGlobalVariables() const {
    return GlobalVariableStates;
  }
  
  /// \brief Get the memory areas occupied by unmapped globals.
  ///
  decltype(UnmappedStaticAreas) const &getUnmappedStaticAreas() const {
    return UnmappedStaticAreas;
  };
  
  /// @} (Global variables.)
  
  
  /// \name Dynamic memory allocations.
  /// @{
  
  /// \brief Get all mapped dynamic memory allocations.
  ///
  std::vector<MallocState> getDynamicMemoryAllocations() const;
  
  /// @} (Dynamic memory allocations.)
  
  
  /// \name Memory state.
  /// @{
  
  /// @} (Memory state.)
  
  
  /// \name Streams.
  /// @{
  
  /// \brief Get the currently open streams.
  ///
  decltype(Streams) const &getStreams() const { return Streams; }
  
  /// \brief Get a pointer to the stream at Address, or nullptr if none exists.
  ///
  StreamState const *getStream(uintptr_t Address) const;
  
  /// @} (Streams.)
};


/// Print a textual description of a ProcessState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State);


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDPROCESSSTATE_HPP
