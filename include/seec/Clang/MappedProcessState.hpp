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

#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedValue.hpp"

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

// Documented in MappedProcessTrace.hpp
namespace cm {

class ThreadState;


/// \brief SeeC-Clang-mapped process state.
///
class ProcessState {
  /// The SeeC-Clang-mapped trace.
  seec::cm::ProcessTrace const &Trace;
  
  /// The base (unmapped) state.
  std::unique_ptr<seec::trace::ProcessState> UnmappedState;
  
  /// Thread states.
  std::vector<std::unique_ptr<seec::cm::ThreadState>> ThreadStates;
  
  /// The current Value store.
  std::shared_ptr<ValueStore const> CurrentValueStore;
  
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
  
  
  /// \name Access underlying information.
  /// @{
  
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
  
  
  /// \name Threads.
  /// @{
  
  /// \brief Get the number of threads.
  std::size_t getThreadCount() const;
  
  /// \brief Get the state of a thread.
  seec::cm::ThreadState &getThread(std::size_t Index);
  
  /// \brief Get the state of a thread.
  seec::cm::ThreadState const &getThread(std::size_t Index) const;
  
  /// @}
  
  
  /// \name Global variables.
  /// @{
  
  /// \brief Get all mapped global variables.
  ///
  std::vector<GlobalVariable> getGlobalVariables() const;
  
  /// @}
  
  
  /// \name Dynamic memory allocations.
  /// @{
  
  /// @}
  
  
  /// \name Memory state.
  /// @{
  
  /// @}
};


/// Print a textual description of a ProcessState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ProcessState const &State);


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDPROCESSSTATE_HPP
