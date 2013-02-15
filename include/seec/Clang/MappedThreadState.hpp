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
/// This file defines ThreadState, a class for viewing a single thread's state
/// in a recreated state from a SeeC-Clang-mapped process trace.
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_MAPPEDTHREADSTATE_HPP
#define SEEC_CLANG_MAPPEDTHREADSTATE_HPP


namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace trace {
  class ThreadState;
} // namespace trace (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class ProcessState;


/// \brief SeeC-Clang-mapped thread state.
///
class ThreadState {
  /// The mapped process state that this thread belongs to.
  ProcessState &Parent;
  
  /// The base (unmapped) state.
  seec::trace::ThreadState &UnmappedState;
  
public:
  /// \brief Constructor.
  ThreadState(ProcessState &WithParent,
              seec::trace::ThreadState &ForState)
  : Parent(WithParent),
    UnmappedState(ForState)
  {}
  
  /// \brief Destructor.
  ~ThreadState();
  
  /// \brief Move constructor.
  ThreadState(ThreadState &&) = default;
  
  /// \brief Move assignment.
  ThreadState &operator=(ThreadState &&) = default;
  
  // No copying.
  ThreadState(ThreadState const &) = delete;
  ThreadState &operator=(ThreadState const &) = delete;
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the underlying (unmapped) state.
  seec::trace::ThreadState &getUnmappedState() { return UnmappedState; }

  /// \brief Get the underlying (unmapped) state.
  seec::trace::ThreadState const &getUnmappedState() const {
    return UnmappedState;
  }
  
  /// @} (Access underlying information.)
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the process state that this thread state belongs to.
  ProcessState &getParent() { return Parent; }
  
  /// \brief Get the process state that this thread state belongs to.
  ProcessState const &getParent() const { return Parent; }
  
  /// @} (Accessors.)
};


/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDTHREADSTATE_HPP
