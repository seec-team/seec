//===- include/seec/Clang/MappedThreadState.hpp ---------------------------===//
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

#include "seec/ICU/Augmenter.hpp"

#include <functional>
#include <memory>
#include <vector>


namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace trace {
  class ThreadState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class FunctionState;
class ProcessState;


/// \brief SeeC-Clang-mapped thread state.
///
class ThreadState {
  /// The mapped process state that this thread belongs to.
  ProcessState &Parent;
  
  /// The base (unmapped) state.
  seec::trace::ThreadState &UnmappedState;
  
  /// The mapped call stack.
  std::vector<std::unique_ptr<FunctionState>> CallStack;
  
  /// References to the mapped FunctionState objects. This is used to expose
  /// the function states to clients without exposing details of the storage
  /// implementation (i.e. unique_ptr).
  std::vector<std::reference_wrapper<FunctionState const>> CallStackRefs;
  
public:
  /// \brief Constructor.
  ///
  ThreadState(ProcessState &WithParent,
              seec::trace::ThreadState &ForState);
  
  /// \brief Destructor.
  ///
  ~ThreadState();
  
  /// \brief Move constructor.
  ///
  ThreadState(ThreadState &&) = default;
  
  /// \brief Move assignment.
  ///
  ThreadState &operator=(ThreadState &&) = default;
  
  // No copying.
  ThreadState(ThreadState const &) = delete;
  ThreadState &operator=(ThreadState const &) = delete;
  
  
  /// \brief Clear any cached information.
  ///
  /// Must be called following updates to the underlying state. May eagerly
  /// generate new information.
  ///
  void cacheClear();
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation,
             AugmentationCallbackFn Augmenter) const;
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the ID of this thread.
  ///
  uint32_t getThreadID() const;
  
  /// \brief Get the underlying (unmapped) state.
  ///
  seec::trace::ThreadState &getUnmappedState() { return UnmappedState; }

  /// \brief Get the underlying (unmapped) state.
  ///
  seec::trace::ThreadState const &getUnmappedState() const {
    return UnmappedState;
  }
  
  /// @} (Access underlying information.)
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the process state that this thread state belongs to.
  ///
  ProcessState &getParent() { return Parent; }
  
  /// \brief Get the process state that this thread state belongs to.
  ///
  ProcessState const &getParent() const { return Parent; }
  
  /// @} (Accessors.)
  
  
  /// \name Queries.
  /// @{
  
  /// \brief Check if this thread is at the beginning of its trace.
  /// \return true iff this thread has not added any events.
  ///
  bool isAtStart() const;
  
  /// \brief Check if this thread is at the end of its trace.
  /// \return true iff this thread has no more events to add.
  ///
  bool isAtEnd() const;
  
  /// @}
  
  
  /// \name Call stack.
  /// @{
  
private:
  /// \brief Generate the mapped call stack.
  ///
  void generateCallStack();

public:
  /// \brief Get the function state of all functions on the call stack.
  ///
  std::vector<std::reference_wrapper<FunctionState const>> const &
  getCallStack() const {
    return CallStackRefs;
  }
  
  /// @} (Call stack.)
};


/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDTHREADSTATE_HPP
