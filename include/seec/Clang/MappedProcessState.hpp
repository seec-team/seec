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

#include "seec/Clang/MappedProcessTrace.hpp"

#include <memory>


namespace llvm {
  class raw_ostream;
}

namespace seec {

namespace trace {
  class ProcessState;
}

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
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the synthetic process time for this state.
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
