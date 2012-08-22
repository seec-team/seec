//===- include/seec/Trace/ThreadState.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_THREADSTATE_HPP
#define SEEC_TRACE_THREADSTATE_HPP

#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Maybe.hpp"

#include <memory>
#include <vector>

namespace seec {

namespace trace {

class ProcessState; // forward-declare for ThreadState


/// \brief State of a thread at a specific point in time.
///
class ThreadState {
  friend class ProcessState; // Allow ProcessStates to construct ThreadStates.


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
  EventReference NextEvent;

  /// The synthetic process time that this ThreadState represents.
  uint64_t ProcessTime;

  /// The synthetic thread time that this ThreadState represents.
  uint64_t ThreadTime;

  /// The stack of FunctionState objects.
  std::vector<FunctionState> CallStack;
  
  /// The runtime error at this point of time in the thread (if any).
  std::unique_ptr<seec::runtime_errors::RunError> CurrentError;

  /// @}


  /// \brief Constructor.
  ThreadState(ProcessState &Parent,
              ThreadTrace const &Trace);

  // Don't allow copying
  ThreadState(ThreadState const &Other) = delete;
  ThreadState &operator=(ThreadState const &RHS) = delete;


  /// \name Movement
  /// @{

  void addEvent(EventRecord<EventType::None> const &);
  void addEvent(EventRecord<EventType::FunctionStart> const &);
  void addEvent(EventRecord<EventType::FunctionEnd> const &);
  void addEvent(EventRecord<EventType::BasicBlockStart> const &);
  void addEvent(EventRecord<EventType::NewProcessTime> const &);
  void addEvent(EventRecord<EventType::PreInstruction> const &);
  void addEvent(EventRecord<EventType::Instruction> const &);
  void addEvent(EventRecord<EventType::InstructionWithSmallValue> const &);
  void addEvent(EventRecord<EventType::InstructionWithValue> const &);
  void addEvent(EventRecord<EventType::InstructionWithLargeValue> const &);
  void addEvent(EventRecord<EventType::StackRestore> const &);
  void addEvent(EventRecord<EventType::Alloca> const &);
  void addEvent(EventRecord<EventType::Malloc> const &);
  void addEvent(EventRecord<EventType::Free> const &);
  void addEvent(EventRecord<EventType::StateTyped> const &);
  void addEvent(EventRecord<EventType::StateUntypedSmall> const &);
  void addEvent(EventRecord<EventType::StateUntyped> const &);
  void addEvent(EventRecord<EventType::StateClear> const &);
  void addEvent(EventRecord<EventType::RuntimeError> const &);

  /// Swallows unmatched calls to addEvent. This allows us to restrict calls to
  /// addEvent using an if statement when switching over types that do not have
  /// a defined addEvent (i.e. types with the trait is_subservient). Calling
  /// this function at runtime is an error.
  template<typename T>
  void addEvent(T &&Object) { llvm_unreachable("addEvent(...) called!"); }

  /// Add the event referenced by NextEvent to the state, and then increment
  /// NextEvent.
  void addNextEvent();

  void addNextEventBlock();

  void makePreviousInstructionActive(EventReference PriorTo);

  void removeEvent(EventRecord<EventType::None> const &);
  void removeEvent(EventRecord<EventType::FunctionStart> const &);
  void removeEvent(EventRecord<EventType::FunctionEnd> const &);
  void removeEvent(EventRecord<EventType::BasicBlockStart> const &);
  void removeEvent(EventRecord<EventType::NewProcessTime> const &);
  void removeEvent(EventRecord<EventType::PreInstruction> const &);
  void removeEvent(EventRecord<EventType::Instruction> const &);
  void removeEvent(EventRecord<EventType::InstructionWithSmallValue> const &);
  void removeEvent(EventRecord<EventType::InstructionWithValue> const &);
  void removeEvent(EventRecord<EventType::InstructionWithLargeValue> const &);
  void removeEvent(EventRecord<EventType::StackRestore> const &);
  void removeEvent(EventRecord<EventType::Alloca> const &);
  void removeEvent(EventRecord<EventType::Malloc> const &);
  void removeEvent(EventRecord<EventType::Free> const &);
  void removeEvent(EventRecord<EventType::StateTyped> const &);
  void removeEvent(EventRecord<EventType::StateUntypedSmall> const &);
  void removeEvent(EventRecord<EventType::StateUntyped> const &);
  void removeEvent(EventRecord<EventType::StateClear> const &);
  void removeEvent(EventRecord<EventType::StateOverwritten> const &);
  void removeEvent(EventRecord<EventType::StateOverwrittenFragment> const &);
  void removeEvent(EventRecord<EventType::RuntimeError> const &);

  /// Swallows unmatched calls to removeEvent. This allows us to restrict calls
  /// to removeEvent using an if statement when switching over types that do
  /// not have a defined removeEvent (i.e. types with the trait is_subservient).
  /// Calling this function at runtime is an error.
  void removeEvent(...) { llvm_unreachable("removeEvent(...) called!"); }

  /// Decrement NextEvent, and then remove the event it references from the
  /// state.
  void removePreviousEvent();

  void removePreviousEventBlock();

  /// Move this thread's state until it agrees with the given ProcessTime.
  void setProcessTime(uint64_t ProcessTime);

  /// @} (Movement)

public:
  /// \name Accessors
  /// @{

  /// Get the ThreadTrace for this thread.
  ThreadTrace const &getTrace() const { return Trace; }
  
  /// Get the next event to process when moving forward through the trace.
  EventReference getNextEvent() const { return NextEvent; }

  /// Get the synthetic thread time that this ThreadState represents.
  uint64_t getThreadTime() const { return ThreadTime; }

  /// Get the current stack of FunctionStates.
  decltype(CallStack) const &getCallStack() const { return CallStack; }
  
  /// \brief Get a pointer to the current RunError for this thread.
  /// \return a pointer to the current RunError for this thread, if one exists,
  ///         otherwise nullptr.
  seec::runtime_errors::RunError const *getCurrentError() const {
    return CurrentError.get();
  }

  /// @} (Accessors)


  /// \name Mutators
  /// @{

  /// \brief Move the state until this thread reaches the given thread time.
  ///
  /// TODO: Enable for multi-threaded traces (the process time may need to be
  /// updated by other thread traces).
  ///
  /// \param NewThreadTime The new thread time for this thread.
  void setThreadTime(uint64_t NewThreadTime);

  /// @}


  /// \name Searching
  /// @{

  /// Get the last event that modified the shared process state from this
  /// thread.
  seec::util::Maybe<EventReference> getLastProcessModifier() const;

  /// @}
};

/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_THREADSTATE_HPP
