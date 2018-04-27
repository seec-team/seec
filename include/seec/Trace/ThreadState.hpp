//===- include/seec/Trace/ThreadState.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_THREADSTATE_HPP
#define SEEC_TRACE_THREADSTATE_HPP

#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/StateCommon.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <memory>
#include <type_traits>
#include <vector>
#include <deque>

namespace seec {

namespace trace {

class EventReference;
class FunctionState;
class ProcessState; // forward-declare for ThreadState
class ThreadTrace;
struct ThreadStateMoverImpl;


/// \brief State of a thread at a specific point in time.
///
class ThreadState {
  friend struct seec::trace::ThreadStateMoverImpl;


  /// \name Constants
  /// @{

  /// The ProcessState that this ThreadState belongs to.
  ProcessState &Parent;

  /// The ThreadTrace that this ThreadState was produced from.
  ThreadTrace const &Trace;

  /// @}


  /// \name Variables
  /// @{

  /// The next event to process when moving forward through the trace.
  std::unique_ptr<EventReference> m_NextEvent;

  /// The synthetic process time that this ThreadState represents.
  uint64_t ProcessTime;

  /// The synthetic thread time that this ThreadState represents.
  uint64_t ThreadTime;

  /// The stack of FunctionState objects.
  std::vector<std::unique_ptr<FunctionState>> CallStack;
  
  /// Completed functions.
  std::deque<std::unique_ptr<FunctionState>> m_CompletedFunctions;

  /// @}


public:
  /// \brief Constructor.
  ThreadState(ProcessState &Parent,
              ThreadTrace const &Trace);
  
  /// \brief Destructor.
  ~ThreadState();

  // Don't allow copying
  ThreadState(ThreadState const &Other) = delete;
  ThreadState &operator=(ThreadState const &RHS) = delete;


  /// \name Accessors
  /// @{
  
  /// \brief Get the ProcessState that this ThreadState belongs to.
  ProcessState &getParent() { return Parent; }
  
  /// \brief Get the ProcessState that this ThreadState belongs to.
  ProcessState const &getParent() const { return Parent; }

  /// \brief Get the ThreadTrace for this thread.
  ThreadTrace const &getTrace() const { return Trace; }
  
  /// \brief Get the next event to process when moving forward through the
  /// trace.
  EventReference const &getNextEvent() const { return *m_NextEvent; }
  
  void incrementNextEvent();
  
  void decrementNextEvent();

  /// \brief Get the synthetic thread time that this ThreadState represents.
  uint64_t getThreadTime() const { return ThreadTime; }

  /// \brief Get the current stack of FunctionStates.
  decltype(CallStack) const &getCallStack() const { return CallStack; }

  /// @} (Accessors)
  
  
  /// \name Queries.
  /// @{
  
  /// \brief Get the ID of this thread.
  ///
  uint32_t getThreadID() const;
  
  /// \brief Get a pointer to the active function's state, if there is one.
  ///
  FunctionState const *getActiveFunction() const {
    return CallStack.empty() ? nullptr : CallStack.back().get();
  }
  
  /// \brief Check if this thread is at the beginning of its trace.
  /// \return true iff this thread has not added any events.
  ///
  bool isAtStart() const;
  
  /// \brief Check if this thread is at the end of its trace.
  /// \return true iff this thread has no more events to add.
  ///
  bool isAtEnd() const;
  
  /// @} (Queries.)
};

/// \brief Print a comparable textual description of a \c ThreadState.
///
void printComparable(llvm::raw_ostream &Out, ThreadState const &State);

/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_THREADSTATE_HPP
