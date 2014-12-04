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
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/StateCommon.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <memory>
#include <type_traits>
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
  std::vector<std::unique_ptr<FunctionState>> CallStack;

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
  void addEvent(EventRecord<EventType::TraceEnd> const &);
  void addEvent(EventRecord<EventType::FunctionStart> const &);
  void addEvent(EventRecord<EventType::FunctionEnd> const &);
  void addEvent(EventRecord<EventType::BasicBlockStart> const &);
  void addEvent(EventRecord<EventType::NewProcessTime> const &);
  void addEvent(EventRecord<EventType::NewThreadTime> const &);
  void addEvent(EventRecord<EventType::PreInstruction> const &);
  void addEvent(EventRecord<EventType::Instruction> const &);
  void addEvent(EventRecord<EventType::InstructionWithUInt8> const &);
  void addEvent(EventRecord<EventType::InstructionWithUInt16> const &);
  void addEvent(EventRecord<EventType::InstructionWithUInt32> const &);
  void addEvent(EventRecord<EventType::InstructionWithUInt64> const &);
  void addEvent(EventRecord<EventType::InstructionWithPtr> const &);
  void addEvent(EventRecord<EventType::InstructionWithFloat> const &);
  void addEvent(EventRecord<EventType::InstructionWithDouble> const &);
  void addEvent(EventRecord<EventType::InstructionWithLongDouble> const &);
  void addEvent(EventRecord<EventType::StackRestore> const &);
  void addEvent(EventRecord<EventType::Alloca> const &);
  void addEvent(EventRecord<EventType::Malloc> const &);
  void addEvent(EventRecord<EventType::Free> const &);
  void addEvent(EventRecord<EventType::Realloc> const &);
  void addEvent(EventRecord<EventType::StateTyped> const &);
  void addEvent(EventRecord<EventType::StateUntypedSmall> const &);
  void addEvent(EventRecord<EventType::StateUntyped> const &);
  void addEvent(EventRecord<EventType::StateMemmove> const &);
  void addEvent(EventRecord<EventType::StateClear> const &);
  void addEvent(EventRecord<EventType::KnownRegionAdd> const &);
  void addEvent(EventRecord<EventType::KnownRegionRemove> const &);
  void addEvent(EventRecord<EventType::ByValRegionAdd> const &);
  void addEvent(EventRecord<EventType::FileOpen> const &);
  void addEvent(EventRecord<EventType::FileWrite> const &);
  void addEvent(EventRecord<EventType::FileWriteFromMemory> const &);
  void addEvent(EventRecord<EventType::FileClose> const &);
  void addEvent(EventRecord<EventType::DirOpen> const &);
  void addEvent(EventRecord<EventType::DirClose> const &);
  void addEvent(EventRecord<EventType::RuntimeError> const &);

  /// Swallows unmatched calls to addEvent. This allows us to restrict calls to
  /// addEvent using an if statement when switching over types that do not have
  /// a defined addEvent (i.e. types with the trait is_subservient). Calling
  /// this function at runtime is an error.
  void addEvent(...) { llvm_unreachable("addEvent(...) called!"); }

  /// Special handling when re-adding the following, so that they do not set
  /// the thread time.
  void readdEvent(EventRecord<EventType::NewThreadTime> const &);
  void readdEvent(EventRecord<EventType::PreInstruction> const &);
  void readdEvent(EventRecord<EventType::Instruction> const &);
  void readdEvent(EventRecord<EventType::InstructionWithUInt8> const &);
  void readdEvent(EventRecord<EventType::InstructionWithUInt16> const &);
  void readdEvent(EventRecord<EventType::InstructionWithUInt32> const &);
  void readdEvent(EventRecord<EventType::InstructionWithUInt64> const &);
  void readdEvent(EventRecord<EventType::InstructionWithPtr> const &);
  void readdEvent(EventRecord<EventType::InstructionWithFloat> const &);
  void readdEvent(EventRecord<EventType::InstructionWithDouble> const &);
  void readdEvent(EventRecord<EventType::InstructionWithLongDouble> const &);

  /// Special handling when re-adding Allocas during the implementation of
  /// another event (avoids adding the associated allocation to MemoryState).
  void readdEvent(EventRecord<EventType::StackRestore> const &);
  void readdEvent(EventRecord<EventType::Alloca> const &);
  void readdEvent(EventRecord<EventType::ByValRegionAdd> const &);

  template<EventType ET>
  void readdEvent(EventRecord<ET> const &Ev, ...) { addEvent(Ev); }

public:
  /// Add the event referenced by NextEvent to the state, and then increment
  /// NextEvent.
  void addNextEvent();

private:
  void makePreviousInstructionActive(EventReference PriorTo);
  void setPreviousViewOfProcessTime(EventReference PriorTo);

  void removeEvent(EventRecord<EventType::None> const &);
  void removeEvent(EventRecord<EventType::TraceEnd> const &);
  void removeEvent(EventRecord<EventType::FunctionStart> const &);
  void removeEvent(EventRecord<EventType::FunctionEnd> const &);
  void removeEvent(EventRecord<EventType::BasicBlockStart> const &);
  void removeEvent(EventRecord<EventType::NewProcessTime> const &);
  void removeEvent(EventRecord<EventType::NewThreadTime> const &);
  void removeEvent(EventRecord<EventType::PreInstruction> const &);
  void removeEvent(EventRecord<EventType::Instruction> const &);
  void removeEvent(EventRecord<EventType::InstructionWithUInt8> const &);
  void removeEvent(EventRecord<EventType::InstructionWithUInt16> const &);
  void removeEvent(EventRecord<EventType::InstructionWithUInt32> const &);
  void removeEvent(EventRecord<EventType::InstructionWithUInt64> const &);
  void removeEvent(EventRecord<EventType::InstructionWithPtr> const &);
  void removeEvent(EventRecord<EventType::InstructionWithFloat> const &);
  void removeEvent(EventRecord<EventType::InstructionWithDouble> const &);
  void removeEvent(EventRecord<EventType::InstructionWithLongDouble> const &);
  void removeEvent(EventRecord<EventType::StackRestore> const &);
  void removeEvent(EventRecord<EventType::Alloca> const &);
  void removeEvent(EventRecord<EventType::Malloc> const &);
  void removeEvent(EventRecord<EventType::Free> const &);
  void removeEvent(EventRecord<EventType::Realloc> const &);
  void removeEvent(EventRecord<EventType::StateTyped> const &);
  void removeEvent(EventRecord<EventType::StateUntypedSmall> const &);
  void removeEvent(EventRecord<EventType::StateUntyped> const &);
  void removeEvent(EventRecord<EventType::StateMemmove> const &);
  void removeEvent(EventRecord<EventType::StateClear> const &);
  void removeEvent(EventRecord<EventType::KnownRegionAdd> const &);
  void removeEvent(EventRecord<EventType::KnownRegionRemove> const &);
  void removeEvent(EventRecord<EventType::ByValRegionAdd> const &);
  void removeEvent(EventRecord<EventType::FileOpen> const &);
  void removeEvent(EventRecord<EventType::FileWrite> const &);
  void removeEvent(EventRecord<EventType::FileWriteFromMemory> const &);
  void removeEvent(EventRecord<EventType::FileClose> const &);
  void removeEvent(EventRecord<EventType::DirOpen> const &);
  void removeEvent(EventRecord<EventType::DirClose> const &);
  void removeEvent(EventRecord<EventType::RuntimeError> const &);

  /// Swallows unmatched calls to removeEvent. This allows us to restrict calls
  /// to removeEvent using an if statement when switching over types that do
  /// not have a defined removeEvent (i.e. types with the trait is_subservient).
  /// Calling this function at runtime is an error.
  void removeEvent(...) { llvm_unreachable("removeEvent(...) called!"); }

public:
  /// Decrement NextEvent, and then remove the event it references from the
  /// state.
  void removePreviousEvent();

  /// @} (Movement)


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
  EventReference getNextEvent() const { return NextEvent; }

  /// \brief Get the synthetic thread time that this ThreadState represents.
  uint64_t getThreadTime() const { return ThreadTime; }

  /// \brief Get the current stack of FunctionStates.
  decltype(CallStack) const &getCallStack() const { return CallStack; }

  /// @} (Accessors)
  
  
  /// \name Queries.
  /// @{
  
  /// \brief Get the ID of this thread.
  ///
  uint32_t getThreadID() const { return Trace.getThreadID(); }
  
  /// \brief Get a pointer to the active function's state, if there is one.
  ///
  FunctionState const *getActiveFunction() const {
    return CallStack.empty() ? nullptr : CallStack.back().get();
  }
  
  /// \brief Check if this thread is at the beginning of its trace.
  /// \return true iff this thread has not added any events.
  ///
  bool isAtStart() const { return NextEvent == Trace.events().begin(); }
  
  /// \brief Check if this thread is at the end of its trace.
  /// \return true iff this thread has no more events to add.
  ///
  bool isAtEnd() const { return NextEvent == Trace.events().end(); }
  
  /// @} (Queries.)
  
  
  /// \name Memory.
  /// @{
  
  /// \brief Find the allocated range that owns an address.
  ///
  /// This method will search:
  ///  - Functions' stack allocations.
  ///
  seec::Maybe<MemoryArea>
  getContainingMemoryArea(stateptr_ty Address) const;
  
  /// @} (Memory.)
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
