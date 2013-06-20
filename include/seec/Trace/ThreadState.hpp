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


  /// \name Memory state event movement.
  /// @{
  
  void addMemoryState(EventLocation const &EvLoc,
                      EventRecord<EventType::StateTyped> const &Ev,
                      MemoryState &Memory);
  
  void restoreMemoryState(EventLocation const &EvLoc,
                          EventRecord<EventType::StateTyped> const &Ev,
                          MemoryState &Memory) {
    addMemoryState(EvLoc, Ev, Memory);
  }
  
  void restoreMemoryState(EventLocation const &EvLoc,
                          EventRecord<EventType::StateTyped> const &Ev,
                          MemoryArea const &InArea,
                          MemoryState &Memory);

  void addMemoryState(EventLocation const &EvLoc,
                      EventRecord<EventType::StateUntypedSmall> const &Ev,
                      MemoryState &Memory);
  
  void
  restoreMemoryState(EventLocation const &EvLoc,
                     EventRecord<EventType::StateUntypedSmall> const &Ev,
                     MemoryState &Memory) {
    addMemoryState(EvLoc, Ev, Memory);
  }
  
  void
  restoreMemoryState(EventLocation const &EvLoc,
                     EventRecord<EventType::StateUntypedSmall> const &Ev,
                     MemoryArea const &InArea,
                     MemoryState &Memory);

  void addMemoryState(EventLocation const &EvLoc,
                      EventRecord<EventType::StateUntyped> const &Ev,
                      MemoryState &Memory);
  
  void restoreMemoryState(EventLocation const &EvLoc,
                          EventRecord<EventType::StateUntyped> const &Ev,
                          MemoryState &Memory) {
    addMemoryState(EvLoc, Ev, Memory);
  }
  
  void restoreMemoryState(EventLocation const &EvLoc,
                          EventRecord<EventType::StateUntyped> const &Ev,
                          MemoryArea const &InArea,
                          MemoryState &Memory);
  
  void addMemoryState(EventLocation const &EvLoc,
                      EventRecord<EventType::StateMemmove> const &Ev,
                      MemoryState &Memory);
  
  void restoreMemoryState(EventLocation const &EvLoc,
                          EventRecord<EventType::StateMemmove> const &Ev,
                          MemoryState &Memory);
  
  void restoreMemoryState(EventLocation const &EvLoc,
                          EventRecord<EventType::StateMemmove> const &Ev,
                          MemoryArea const &InArea,
                          MemoryState &Memory);
  
  template<EventType ET>
  void addMemoryState(
          EventLocation const &EvLoc,
          EventRecord<ET> const &Ev,
          MemoryState &Memory,
          typename std::enable_if<!is_memory_state<ET>::value>::type* = nullptr)
  {
    llvm::errs() << "\ncalled addMemoryState/3 for EventType "
                 << describe(ET) << "\n";
    exit(EXIT_FAILURE);
  }
  
  template<EventType ET>
  void restoreMemoryState(
          EventLocation const &EvLoc,
          EventRecord<ET> const &Ev,
          MemoryState &Memory,
          typename std::enable_if<!is_memory_state<ET>::value>::type* = nullptr)
  {
    llvm::errs() << "\ncalled restoreMemoryState/3 for EventType "
                 << describe(ET) << "\n";
    exit(EXIT_FAILURE);
  }
  
  template<EventType ET>
  void restoreMemoryState(
          EventLocation const &EvLoc,
          EventRecord<ET> const &Ev,
          MemoryArea const &InArea,
          MemoryState &Memory,
          typename std::enable_if<!is_memory_state<ET>::value>::type* = nullptr)
  {
    llvm::errs() << "\ncalled restoreMemoryState/4 for EventType "
                 << describe(ET) << "\n";
    exit(EXIT_FAILURE);
  }
  
  void restoreMemoryState(EventLocation const &Ev,
                          MemoryState &Memory);
  
  void restoreMemoryState(EventLocation const &Ev,
                          MemoryState &Memory,
                          MemoryArea const &InArea);
  
  /// @}
  

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
  void addEvent(EventRecord<EventType::StateMemmove> const &);
  void addEvent(EventRecord<EventType::StateClear> const &);
  void addEvent(EventRecord<EventType::KnownRegionAdd> const &);
  void addEvent(EventRecord<EventType::KnownRegionRemove> const &);
  void addEvent(EventRecord<EventType::ByValRegionAdd> const &);
  void addEvent(EventRecord<EventType::RuntimeError> const &);

  /// Swallows unmatched calls to addEvent. This allows us to restrict calls to
  /// addEvent using an if statement when switching over types that do not have
  /// a defined addEvent (i.e. types with the trait is_subservient). Calling
  /// this function at runtime is an error.
  void addEvent(...) { llvm_unreachable("addEvent(...) called!"); }

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
  void removeEvent(EventRecord<EventType::InstructionWithSmallValue> const &);
  void removeEvent(EventRecord<EventType::InstructionWithValue> const &);
  void removeEvent(EventRecord<EventType::InstructionWithLargeValue> const &);
  void removeEvent(EventRecord<EventType::StackRestore> const &);
  void removeEvent(EventRecord<EventType::Alloca> const &);
  void removeEvent(EventRecord<EventType::Malloc> const &);
  void removeEvent(EventRecord<EventType::Free> const &);
  bool removeEventIfOverwrite(EventReference EvRef);
  void removeEvent(EventRecord<EventType::StateTyped> const &);
  void removeEvent(EventRecord<EventType::StateUntypedSmall> const &);
  void removeEvent(EventRecord<EventType::StateUntyped> const &);
  void removeEvent(EventRecord<EventType::StateMemmove> const &);
  void removeEvent(EventRecord<EventType::StateClear> const &);
  void removeEvent(EventRecord<EventType::StateOverwrite> const &);
  void removeEvent(EventRecord<EventType::StateOverwriteFragment> const &);
  void removeEvent(
    EventRecord<EventType::StateOverwriteFragmentTrimmedRight> const &);
  void removeEvent(
    EventRecord<EventType::StateOverwriteFragmentTrimmedLeft> const &);
  void removeEvent(EventRecord<EventType::StateOverwriteFragmentSplit> const &);
  void removeEvent(EventRecord<EventType::KnownRegionAdd> const &);
  void removeEvent(EventRecord<EventType::KnownRegionRemove> const &);
  void removeEvent(EventRecord<EventType::ByValRegionAdd> const &);
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
  getContainingMemoryArea(uintptr_t Address) const;
  
  /// @} (Memory.)


  /// \name Searching
  /// @{

  /// \brief Get the last event that modified the shared process state from this
  /// thread.
  seec::Maybe<EventReference> getLastProcessModifier() const;

  /// @}
};

/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_THREADSTATE_HPP
