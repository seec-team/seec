#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/Util/Dispatch.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

//------------------------------------------------------------------------------
// ThreadState
//------------------------------------------------------------------------------

ThreadState::ThreadState(ProcessState &Parent,
                         ThreadTrace const &Trace)
: Parent(Parent),
  Trace(Trace),
  NextEvent(Trace.events().begin()),
  ProcessTime(Parent.getProcessTime()),
  ThreadTime(0),
  CallStack(),
  CurrentError()
{}


//------------------------------------------------------------------------------
// Adding events
//------------------------------------------------------------------------------

void ThreadState::addEvent(EventRecord<EventType::None> const &Ev) {}

void ThreadState::addEvent(EventRecord<EventType::FunctionStart> const &Ev) {
  auto const RecordOffset = Ev.getRecord();
  auto const Info = Trace.getFunctionTrace(RecordOffset);
  auto const Index = Info.getIndex();
  
  auto const MappedFunction = Parent.getModule().getFunctionIndex(Index);
  assert(MappedFunction && "Couldn't get FunctionIndex");

  auto State = new FunctionState(*this, Index, *MappedFunction, Info);
  assert(State);
  
  CallStack.emplace_back(State);
  
  ThreadTime = Info.getThreadTimeEntered();
}

void ThreadState::addEvent(EventRecord<EventType::FunctionEnd> const &Ev) {
  auto const RecordOffset = Ev.getRecord();
  auto const Info = Trace.getFunctionTrace(RecordOffset);
  auto const Index = Info.getIndex();

  assert(CallStack.size() && "FunctionEnd with empty CallStack");
  assert(CallStack.back()->getIndex() == Index
         && "FunctionEnd does not match currently active function");

  CallStack.pop_back();
  ThreadTime = Info.getThreadTimeExited();
}

void ThreadState::addEvent(EventRecord<EventType::BasicBlockStart> const &Ev) {
  // TODO
}

void ThreadState::addEvent(EventRecord<EventType::NewProcessTime> const &Ev) {
  // Update this thread's view of ProcessTime.
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::PreInstruction> const &Ev) {
  auto const Index = Ev.getIndex();

  auto &FuncState = *(CallStack.back());
  FuncState.setActiveInstruction(Index);

  CurrentError.reset(nullptr);
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(EventRecord<EventType::Instruction> const &Ev) {
  auto const Index = Ev.getIndex();

  auto &FuncState = *(CallStack.back());
  FuncState.setActiveInstruction(Index);

  CurrentError.reset(nullptr);
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithSmallValue> const &Ev) {
  auto const Offset = Trace.events().offsetOf(Ev);
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.getRuntimeValue(Index).set(Offset, Value);
  FuncState.setActiveInstruction(Index);

  CurrentError.reset(nullptr);
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithValue> const &Ev) {
  auto const Offset = Trace.events().offsetOf(Ev);
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.getRuntimeValue(Index).set(Offset, Value);
  FuncState.setActiveInstruction(Index);

  CurrentError.reset(nullptr);
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithLargeValue> const &Ev) {
  // TODO
  llvm_unreachable("not yet implemented");

  CurrentError.reset(nullptr);
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(EventRecord<EventType::StackRestore> const &Ev) {
  EventReference EvRef(Ev);

  // Clear the current allocas.
  auto &FuncState = *(CallStack.back());
  FuncState.getAllocas().clear();

  // Get all of the StackRestoreAlloca records.
  auto const Allocas = getLeadingBlock<EventType::StackRestoreAlloca>
                                      (rangeAfter(Trace.events(), EvRef));

  // Add the restored allocas.
  for (auto const &RestoreAlloca : Allocas) {
    auto const Offset = RestoreAlloca.getAlloca();
    auto &Alloca = Trace.events().eventAtOffset<EventType::Alloca>(Offset);
    addEvent(Alloca);
  }
}

void ThreadState::addEvent(EventRecord<EventType::Alloca> const &Ev) {
  EventReference EvRef(Ev);

  assert(EvRef != Trace.events().begin() && "Malformed event trace");

  // Find the preceding InstructionWithValue event.
  auto const MaybeInstrRef = rfind<EventType::InstructionWithValue>(
                                rangeBeforeIncluding(Trace.events(), EvRef));

  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithValue>();

  // Add Alloca information.
  auto &FuncState = *(CallStack.back());
  FuncState.getAllocas().emplace_back(FuncState,
                                      Instr.getIndex(),
                                      Instr.getValue().UInt64,
                                      Ev.getElementSize(),
                                      Ev.getElementCount());
}

void ThreadState::addEvent(EventRecord<EventType::Malloc> const &Ev) {
  // Find the preceding InstructionWithValue event.
  EventReference EvRef(Ev);

  assert(EvRef != Trace.events().begin() && "Malformed event trace");

  auto const MaybeInstrRef = rfind<EventType::InstructionWithValue>
                                  (rangeBeforeIncluding(Trace.events(), EvRef));

  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithValue>();

  auto const Address = Instr.getValue().UInt64;
  auto const MallocLocation = EventLocation(Trace.getThreadID(),
                                            Trace.events().offsetOf(EvRef));

  // Update the shared ProcessState.
  Parent.Mallocs.insert(std::make_pair(Address,
                                       MallocState(Address,
                                                   Ev.getSize(),
                                                   MallocLocation)));
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::Free> const &Ev) {
  // Find the original Malloc event.
  auto const MallocThreadID = Ev.getMallocThread();
  auto const MallocOffset = Ev.getMallocOffset();

  auto const &ProcTrace = Parent.getTrace();
  assert(MallocThreadID && MallocThreadID <= ProcTrace.getNumThreads() &&
         "Invalid MallocThread in Free record.");

  auto const &MallocThread = ProcTrace.getThreadTrace(MallocThreadID);

  auto const MallocRef = MallocThread.events().referenceToOffset(MallocOffset);

  // Find the Instruction that the Malloc was attached to.
  auto const MaybeInstrRef = rfind<EventType::InstructionWithValue>
                                  (rangeBeforeIncluding(MallocThread.events(),
                                                        MallocRef));
  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithValue>();

  auto const Address = Instr.getValue().UInt64;

  // Update the shared ProcessState.
  Parent.Mallocs.erase(Address);
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateTyped> const &Ev) {
  // TODO

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(
        EventRecord<EventType::StateUntypedSmall> const &Ev) {
  EventReference EvRef(Ev);
  auto const EvLocation = EventLocation(Trace.getThreadID(),
                                        Trace.events().offsetOf(EvRef));

  auto DataPtr = reinterpret_cast<char const *>(&(Ev.getData()));

  Parent.Memory.add(MappedMemoryBlock(Ev.getAddress(), Ev.getSize(), DataPtr),
                    EvLocation);

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateUntyped> const &Ev) {
  EventReference EvRef(Ev);
  auto const EvLocation = EventLocation(Trace.getThreadID(),
                                        Trace.events().offsetOf(EvRef));

  auto Data = Parent.getTrace().getData(Ev.getDataOffset(), Ev.getDataSize());

  Parent.Memory.add(MappedMemoryBlock(Ev.getAddress(),
                                      Ev.getDataSize(),
                                      Data.data()),
                    EvLocation);

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateClear> const &Ev) {
  Parent.Memory.clear(MemoryArea(Ev.getAddress(), Ev.getClearSize()));
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::RuntimeError> const &Ev) {
  auto ErrRange = rangeAfterIncluding(Trace.events(), Ev);
  CurrentError = deserializeRuntimeError(ErrRange);
  assert(CurrentError);
}

void ThreadState::addNextEvent() {
  switch (NextEvent->getType()) {
    // TODO: If is_function_level, assert that a function exists.

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      if (!is_subservient<EventType::NAME>::value) {                           \
        addEvent(NextEvent.get<EventType::NAME>());                            \
      }                                                                        \
      break;
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }

  ++NextEvent;
}

void ThreadState::addNextEventBlock() {
  seec::util::Maybe<ProcessState::ScopedUpdate> SharedUpdate;
  seec::util::Maybe<uint64_t> NewProcessTime;

  while (true) {
    // Make sure we have permission to update the shared state of the
    // ProcessState, if the next event is going to require it.
    if (!SharedUpdate.assigned() && NextEvent->modifiesSharedState()) {
      NewProcessTime = NextEvent->getProcessTime();
      assert(NewProcessTime.assigned());
      SharedUpdate = Parent.getScopedUpdate(NewProcessTime.get<0>() - 1);
    }

    addNextEvent();

    if (NextEvent->isBlockStart())
      break;
  }
}


//------------------------------------------------------------------------------
// Removing events
//------------------------------------------------------------------------------

void ThreadState::makePreviousInstructionActive(EventReference PriorTo) {
  auto &FuncState = *(CallStack.back());

  // Find the previous instruction event that is part of the same function
  // invocation as PriorTo, if there is such an event.
  auto MaybeRef = rfindInFunction(Trace,
                                  rangeBefore(Trace.events(), PriorTo),
                                  [](EventRecordBase const &Ev) -> bool {
                                    return Ev.isInstruction();
                                  });

  if (!MaybeRef.assigned()) {
    FuncState.clearActiveInstruction();
    return;
  }
  
  // Set the previous instruction as active.
  auto MaybeIndex = MaybeRef.get<0>()->getIndex();
  assert(MaybeIndex.assigned());
  FuncState.setActiveInstruction(MaybeIndex.get<0>());
  
  // If there is a runtime error attached to the previous instruction, then
  // it should be set as the current error now.
  auto ErrorSearchRange = EventRange(MaybeRef.get<0>(), PriorTo);
  auto MaybeErrorRef = find<EventType::RuntimeError>(ErrorSearchRange);
  
  if (MaybeErrorRef.assigned()) {
    auto ErrorRef = MaybeErrorRef.get<0>();
    auto ErrorRange = rangeAfterIncluding(ErrorSearchRange, ErrorRef);
    CurrentError = deserializeRuntimeError(ErrorRange);
    assert(CurrentError);
  }
}

void ThreadState::removeEvent(EventRecord<EventType::None> const &Ev) {}

void ThreadState::removeEvent(EventRecord<EventType::FunctionStart> const &Ev) {
  auto const RecordOffset = Ev.getRecord();
  auto const Info = Trace.getFunctionTrace(RecordOffset);
  auto const Index = Info.getIndex();

  assert(CallStack.size() && "Removing FunctionStart with empty CallStack");
  assert(CallStack.back()->getIndex() == Index
         && "Removing FunctionStart does not match currently active function");

  CallStack.pop_back();
  ThreadTime = Info.getThreadTimeEntered() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::FunctionEnd> const &Ev) {
  auto const RecordOffset = Ev.getRecord();
  auto const Info = Trace.getFunctionTrace(RecordOffset);
  auto const Index = Info.getIndex();
  
  auto const MappedFunction = Parent.getModule().getFunctionIndex(Index);
  assert(MappedFunction && "Couldn't get FunctionIndex");

  auto State = new FunctionState(*this, Index, *MappedFunction, Info);
  assert(State);
  
  CallStack.emplace_back(State);
  
  ThreadTime = Info.getThreadTimeExited() - 1;

  // Now we need to restore all function-level events. For now, we use the
  // naive method, which is to simply re-add all events from the start of the
  // function to the end.
  EventReference EvRef(Ev);
  auto RestoreRef = ++(Trace.events().referenceToOffset(Info.getEventStart()));

  for (; RestoreRef != EvRef; ++RestoreRef) {
    // Skip any events belonging to child functions.
    if (RestoreRef->getType() == EventType::FunctionStart) {
      auto const &ChildStartEv = RestoreRef.get<EventType::FunctionStart>();
      auto const ChildRecordOffset = ChildStartEv.getRecord();
      auto const Child = Trace.getFunctionTrace(ChildRecordOffset);
      
      // Set the iterator to the FunctionEnd event for the child function. It
      // will be incremented to the next event in this function, by the loop.
      RestoreRef = Trace.events().referenceToOffset(Child.getEventEnd());
      
      continue;
    }

    switch (RestoreRef->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
      case EventType::NAME:                                                    \
        if (!is_subservient<EventType::NAME>::value                            \
            && is_function_level<EventType::NAME>::value) {                    \
          addEvent(RestoreRef.get<EventType::NAME>());                         \
        }                                                                      \
        break;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
  }
}

void ThreadState::removeEvent(
      EventRecord<EventType::BasicBlockStart> const &Ev) {
  // TODO
}

void ThreadState::removeEvent(
      EventRecord<EventType::NewProcessTime> const &Ev) {
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::removeEvent(
      EventRecord<EventType::PreInstruction> const &Ev) {
  makePreviousInstructionActive(EventReference(Ev));
  ThreadTime = Ev.getThreadTime() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::Instruction> const &Ev) {
  makePreviousInstructionActive(EventReference(Ev));
  ThreadTime = Ev.getThreadTime() - 1;
}

void ThreadState::removeEvent(
      EventRecord<EventType::InstructionWithSmallValue> const &Ev) {
  auto &FuncState = *(CallStack.back());

  auto const PreviousOffset = Ev.getPreviousSame();
  if (PreviousOffset != noOffset()) {
    auto &Prev
      = Trace.events().eventAtOffset<EventType::InstructionWithSmallValue>
                                    (PreviousOffset);
    addEvent(Prev);
  }
  else {
    auto const Index = Ev.getIndex();
    FuncState.getRuntimeValue(Index).clear();
  }

  // Find the previous instruction and set it as the active instruction.
  makePreviousInstructionActive(EventReference(Ev));

  ThreadTime = Ev.getThreadTime() - 1;
}

void ThreadState::removeEvent(
      EventRecord<EventType::InstructionWithValue> const &Ev) {
  auto &FuncState = *(CallStack.back());

  auto const PreviousOffset = Ev.getPreviousSame();
  if (PreviousOffset != noOffset()) {
    auto &Prev = Trace.events().eventAtOffset<EventType::InstructionWithValue>
                                             (PreviousOffset);
    addEvent(Prev);
  }
  else {
    auto const Index = Ev.getIndex();
    FuncState.getRuntimeValue(Index).clear();
  }

  // Find the previous instruction and set it as the active instruction.
  makePreviousInstructionActive(EventReference(Ev));

  ThreadTime = Ev.getThreadTime() - 1;
}

void ThreadState::removeEvent(
      EventRecord<EventType::InstructionWithLargeValue> const &Ev) {
  // TODO
  llvm_unreachable("Not yet implemented.");
}

void ThreadState::removeEvent(EventRecord<EventType::StackRestore> const &Ev) {
  // Clear the current allocas.
  auto &FuncState = *(CallStack.back());
  FuncState.getAllocas().clear();

  auto const PreviousOffset = Ev.getPrevious();

  if (PreviousOffset != noOffset()) {
    auto Events = Trace.events();

    // Add the Allocas that were valid after the previous StackRestore.
    auto &RestoreEv
      = Events.eventAtOffset<EventType::StackRestore>(PreviousOffset);
    addEvent(RestoreEv);

    // Now add all Allocas that occured between the previous StackRestore and
    // the current StackRestore.
    auto PreviousEvRef = Events.referenceToOffset(PreviousOffset);
    EventReference CurrentEvRef(Ev);

    // Iterate through the events, skipping any child functions as we go.
    for (EventReference It(PreviousEvRef); It != CurrentEvRef; ++It) {
      if (It->getType() == EventType::FunctionStart) {
        auto const &StartEv = It.get<EventType::FunctionStart>();
        auto const Info = Trace.getFunctionTrace(StartEv.getRecord());
        It = Events.referenceToOffset(Info.getEventEnd());
        // It will be incremented when we finish this iteration, so the
        // FunctionEnd for this child will (correctly) not be seen.
      }
      else if (It->getType() == EventType::Alloca) {
        addEvent(It.get<EventType::Alloca>());
      }
    }
  }
  else {
    auto FunctionInfo = CallStack.back()->getTrace();
    auto StartOffset = FunctionInfo.getEventStart();

    // Iterate through the events, adding all Allocas until we find
    // the StackRestore, skipping any child functions as we go.
    auto ItEventRef = Trace.events().referenceToOffset(StartOffset);
    EventReference EndEventRef(Ev);

    for (++ItEventRef; ItEventRef != EndEventRef; ++ItEventRef) {
      if (ItEventRef->getType() == EventType::FunctionStart) {
        auto const &StartEv = ItEventRef.get<EventType::FunctionStart>();
        auto const Info = Trace.getFunctionTrace(StartEv.getRecord());
        ItEventRef = Trace.events().referenceToOffset(Info.getEventEnd());
        // It will be incremented when we finish this iteration, so the
        // FunctionEnd for this child will (correctly) not be seen.
      }
      else if (ItEventRef->getType() == EventType::FunctionEnd) {
        break;
      }
      else if (ItEventRef->getType() == EventType::Alloca) {
        addEvent(ItEventRef.get<EventType::Alloca>());
      }
    }
  }
}

void ThreadState::removeEvent(EventRecord<EventType::Alloca> const &Ev) {
  // Remove Alloca information.
  auto &FuncState = *(CallStack.back());
  assert(!FuncState.getAllocas().empty());
  FuncState.getAllocas().pop_back();
}

void ThreadState::removeEvent(EventRecord<EventType::Malloc> const &Ev) {
  // Find the preceding InstructionWithValue event.
  EventReference EvRef(Ev);
  auto const MaybeInstrRef = rfind<EventType::InstructionWithValue>
                                  (rangeBeforeIncluding(Trace.events(), EvRef));
  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithValue>();
  auto const Address = Instr.getValue().UInt64;

  Parent.Mallocs.erase(Address);

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  ProcessTime = Ev.getProcessTime() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::Free> const &Ev) {
  // Find the original Malloc event.
  auto const MallocThreadID = Ev.getMallocThread();
  auto const MallocOffset = Ev.getMallocOffset();

  auto const &ProcTrace = Parent.getTrace();
  assert(MallocThreadID && MallocThreadID <= ProcTrace.getNumThreads() &&
         "Invalid MallocThread in Free record.");

  auto const &MallocThread = ProcTrace.getThreadTrace(MallocThreadID);

  auto const MallocRef = MallocThread.events().referenceToOffset(MallocOffset);
  auto const &MallocEv = MallocRef.get<EventType::Malloc>();

  // Find the Instruction that the Malloc was attached to.
  auto const MaybeInstrRef = rfind<EventType::InstructionWithValue>
                                  (rangeBeforeIncluding(MallocThread.events(),
                                                        MallocRef));
  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithValue>();

  // Information required to recreate the dynamic memory allocation.
  auto const Address = Instr.getValue().UInt64;

  EventLocation MallocLocation(MallocThreadID, MallocOffset);

  // Update the shared ProcessState.
  Parent.Mallocs.insert(std::make_pair(Address,
                                       MallocState(Address,
                                                   MallocEv.getSize(),
                                                   MallocLocation)));

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  ProcessTime = Ev.getProcessTime() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::StateTyped> const &Ev) {
  // Clear this state.
  // TODO.

  // Restore any overwritten states.
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;

    assert(Trace.events().contains(EvRef));

    auto Dispatched = EvRef.dispatch(
      [this]
      (EventRecord<EventType::StateOverwritten> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      },
      [this]
      (EventRecord<EventType::StateOverwrittenFragment> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      });

    assert(Dispatched.assigned() && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  ProcessTime = Ev.getProcessTime() - 1;
}

void ThreadState::removeEvent(
        EventRecord<EventType::StateUntypedSmall> const &Ev) {
  // Clear this state.
  Parent.Memory.clear(MemoryArea(Ev.getAddress(), Ev.getSize()));

  // Restore any overwritten states.
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;

    assert(Trace.events().contains(EvRef));

    auto Dispatched = EvRef.dispatch(
      [this]
      (EventRecord<EventType::StateOverwritten> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      },
      [this]
      (EventRecord<EventType::StateOverwrittenFragment> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      });

    assert(Dispatched.assigned() && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  ProcessTime = Ev.getProcessTime() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::StateUntyped> const &Ev) {
  // Clear this state.
  Parent.Memory.clear(MemoryArea(Ev.getAddress(), Ev.getDataSize()));

  // Restore any overwritten states.
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;

    assert(Trace.events().contains(EvRef));

    auto Dispatched = EvRef.dispatch(
      [this]
      (EventRecord<EventType::StateOverwritten> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      },
      [this]
      (EventRecord<EventType::StateOverwrittenFragment> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      });

    assert(Dispatched.assigned() && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  ProcessTime = Ev.getProcessTime() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::StateClear> const &Ev) {
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;

    assert(Trace.events().contains(EvRef));

    auto Dispatched = EvRef.dispatch(
      [this]
      (EventRecord<EventType::StateOverwritten> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      },
      [this]
      (EventRecord<EventType::StateOverwrittenFragment> const &Ev) -> bool {
        removeEvent(Ev);
        return true;
      });

    assert(Dispatched.assigned() && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  ProcessTime = Ev.getProcessTime() - 1;
}

void ThreadState::removeEvent(
        EventRecord<EventType::StateOverwritten> const &Ev) {
  auto const StateThreadID = Ev.getStateThreadID();

  if (StateThreadID) {
    // Restore state from a previous state event.
    EventLocation const StateLoc(StateThreadID, Ev.getStateOffset());

    auto const StateRef = Parent.getTrace().getEventReference(StateLoc);

    auto const Dispatched = StateRef.dispatch(
#if 0 // TODO: Not yet implemented
      [this] (EventRecord<EventType::StateTyped> const &Ev) -> bool {
        return true;
      },
#endif
      [&, this] (EventRecord<EventType::StateUntypedSmall> const &Ev) -> bool {
        auto const Address = Ev.getAddress();
        auto const Size = Ev.getSize();
        auto const Data = reinterpret_cast<char const *>(&(Ev.getData()));
        Parent.Memory.add(MappedMemoryBlock(Address, Size, Data),
                          StateLoc);
        return true;
      },
      [&, this] (EventRecord<EventType::StateUntyped> const &Ev) -> bool {
        auto const Address = Ev.getAddress();
        auto const Size = Ev.getDataSize();
        auto const Data = Parent.getTrace().getData(Ev.getDataOffset(),
                                                    Ev.getDataSize());
        Parent.Memory.add(MappedMemoryBlock(Address, Size, Data.data()),
                          StateLoc);
        return true;
      });

    assert(Dispatched.assigned() && "Malformed trace!");
  }
  else {
    // Restore state from a global variable's initial data.
    auto const &ProcTrace = Parent.getTrace();
    auto const GVIndex = static_cast<uint32_t>(Ev.getStateOffset());
    auto const GVAddress = ProcTrace.getGlobalVariableAddress(GVIndex);

    // TODO
  }
}

void ThreadState::removeEvent(
        EventRecord<EventType::StateOverwrittenFragment> const &Ev) {
  auto const StateThreadID = Ev.getStateThreadID();

  if (StateThreadID) {
    // Restore state from a previous state event.
    EventLocation StateLoc(StateThreadID, Ev.getStateOffset());

    auto StateRef = Parent.getTrace().getEventReference(StateLoc);

    // (uint64_t, FragmentAddress),
    // (uint64_t, FragmentSize)

    uint64_t DataAddress = 0;
    uint64_t DataSize = 0;
    char const *DataPtr = nullptr;

    auto Dispatched = StateRef.dispatch(
#if 0 // Not yet implemented
      [this] (EventRecord<EventType::StateTyped> const &Ev) -> bool {
        return true;
      },
#endif
      [&, this] (EventRecord<EventType::StateUntypedSmall> const &Ev) -> bool {
        DataAddress = Ev.getAddress();
        DataSize = Ev.getSize();
        DataPtr = reinterpret_cast<char const *>(&(Ev.getData()));
        return true;
      },
      [&, this] (EventRecord<EventType::StateUntyped> const &Ev) -> bool {
        DataAddress = Ev.getAddress();
        DataSize = Ev.getDataSize();
        auto Data = Parent.getTrace().getData(Ev.getDataOffset(),
                                              Ev.getDataSize());
        DataPtr = Data.data();
        return true;
      });

    assert(Dispatched.assigned() && "Malformed trace!");

    auto const FragmentAddress = Ev.getFragmentAddress();
    auto const FragmentSize = Ev.getFragmentSize();

    if (FragmentAddress > DataAddress)
      DataPtr += (FragmentAddress - DataAddress);

    // TODO: join fragments that match neighbouring fragments!
    Parent.Memory.add(MappedMemoryBlock(FragmentAddress, FragmentSize, DataPtr),
                      StateLoc);
  }
  else {
    // Restore state from a global variable's initial data.
    auto const &ProcTrace = Parent.getTrace();
    auto const GVIndex = static_cast<uint32_t>(Ev.getStateOffset());
    auto const GVAddress = ProcTrace.getGlobalVariableAddress(GVIndex);

    // TODO
  }
}

void ThreadState::removeEvent(EventRecord<EventType::RuntimeError> const &Ev) {
  CurrentError.reset(nullptr);
}

void ThreadState::removePreviousEvent() {
  --NextEvent;

  switch (NextEvent->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      if (!is_subservient<EventType::NAME>::value)                             \
        removeEvent(NextEvent.get<EventType::NAME>());                         \
      break;
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
}

void ThreadState::removePreviousEventBlock() {
  seec::util::Maybe<ProcessState::ScopedUpdate> SharedUpdate;
  seec::util::Maybe<uint64_t> NewProcessTime;

  while (NextEvent != Trace.events().begin()) {
    auto PreviousEvent = NextEvent;
    --PreviousEvent;

    // Make sure we have permission to update the shared state of the
    // ProcessState, if the next event is going to require it.
    if (!SharedUpdate.assigned() && PreviousEvent->modifiesSharedState()) {
      NewProcessTime = PreviousEvent->getProcessTime();
      assert(NewProcessTime.assigned());
      SharedUpdate = Parent.getScopedUpdate(NewProcessTime.get<0>());
    }

    removePreviousEvent();

    if (NextEvent->isBlockStart())
      break;
  }
}

void ThreadState::setProcessTime(uint64_t NewProcessTime) {
  if (NewProcessTime == ProcessTime) {
    return;
  }

  if (NewProcessTime > ProcessTime) {
    // Moving forward.
    EventRange Events = rangeAfterIncluding(Trace.events(), NextEvent);

    // Find first event Ev with Ev.getProcessTime() >= NewProcessTime.
    auto MaybeEventRef = find(Events,
                              [=](EventRecordBase const &Ev) -> bool {
                                auto ProcessTime = Ev.getProcessTime();
                                if (!ProcessTime.assigned())
                                  return false;
                                return ProcessTime.get<0>() >= NewProcessTime;
                              });

    EventRange EventsToApply;

    if (MaybeEventRef.assigned()) {
      auto &TheEventRef = MaybeEventRef.get<0>();
      auto TheProcessTime = TheEventRef->getProcessTime().get<0>();

      if (TheProcessTime == NewProcessTime) {
        // If the event matches the new process time, then we should apply all
        // blocks up to and including the event.

        // Find the start of the next block.
        auto MaybeNext = find(rangeAfter(Events, TheEventRef),
                              [=](EventRecordBase const &Ev) -> bool {
                                return Ev.isBlockStart();
                              });

        if (MaybeNext.assigned()) {
          // Select the events in all prior blocks and the containing block.
          EventsToApply = rangeBefore(Events, MaybeNext.get<0>());
        }
        else {
          // Select all events (no block is following the containing block).
          EventsToApply = Events;
        }
      }
      else {
        // If the event is higher than the new process time, then we should
        // apply all blocks up to (but not including) the event.

        // Find the start of the containing block.
        auto MaybeStart = rfind(rangeBeforeIncluding(Events, TheEventRef),
                                [=](EventRecordBase const &Ev) -> bool {
                                  return Ev.isBlockStart();
                                });

        if (MaybeStart.assigned()) {
          // Select the events in all prior blocks.
          EventsToApply = rangeBefore(Events, MaybeStart.get<0>());
        }
        else {
          // There are no blocks to apply.
          EventsToApply = EventRange(NextEvent, NextEvent);
        }
      }
    }
    else {
      // Apply all remaining events in the thread trace (if any).
      EventsToApply = Events;
    }

    // Apply the selected events.
    if (!EventsToApply.empty()) {
      while (NextEvent != EventsToApply.end()) {
        addNextEventBlock();
      }
    }
  }
  else {
    // Moving backward.
    EventRange Events = rangeBefore(Trace.events(), NextEvent);

    // Find first event Ev with Ev.getProcessTime() <= NewProcessTime.
    auto MaybeEventRef = rfind(Events,
                               [=](EventRecordBase const &Ev) -> bool {
                                 auto ProcessTime = Ev.getProcessTime();
                                 if (!ProcessTime.assigned())
                                   return false;
                                 return ProcessTime.get<0>() <= NewProcessTime;
                               });

    EventRange EventsToRemove;

    if (MaybeEventRef.assigned()) {
      // Apply all blocks down to (but not including) the event.
      auto &TheEventRef = MaybeEventRef.get<0>();

      // Find the start of the next block.
      auto MaybeNext = find(rangeAfter(Events, TheEventRef),
                            [=](EventRecordBase const &Ev) -> bool {
                              return Ev.isBlockStart();
                            });

      if (MaybeNext.assigned()) {
        // Select the events in all blocks following the containing block.
        EventsToRemove = rangeAfterIncluding(Events, MaybeNext.get<0>());
      }
      else {
        // There are no blocks to apply.
        EventsToRemove = EventRange(NextEvent, NextEvent);
      }
    }
    else {
      // Apply all remaining events in the thread trace (if any).
      EventsToRemove = Events;
    }

    // Apply the selected events.
    if (!EventsToRemove.empty()) {
      while (NextEvent != EventsToRemove.begin()) {
        removePreviousEventBlock();
      }
    }
  }
}

void ThreadState::setThreadTime(uint64_t NewThreadTime) {
  if (ThreadTime == NewThreadTime)
    return;

  if (ThreadTime < NewThreadTime) {
    // Move forward
    auto LastEvent = Trace.events().end();

    while (ThreadTime < NewThreadTime && NextEvent != LastEvent) {
      addNextEventBlock();
    }
  }
  else {
    // Move backward
    auto FirstEvent = Trace.events().begin();

    while (ThreadTime > NewThreadTime && NextEvent != FirstEvent) {
      removePreviousEventBlock();
    }
  }

  assert(ThreadTime == NewThreadTime);
}

seec::util::Maybe<EventReference> ThreadState::getLastProcessModifier() const {
  return rfind(rangeBefore(Trace.events(), NextEvent),
               [](EventRecordBase const &Ev){return Ev.modifiesSharedState();});
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State) {
  Out << " Thread #" << State.getTrace().getThreadID()
      << " @TT=" << State.getThreadTime()
      << "\n";
  
  if (State.getCurrentError())
    Out << "  With RunError\n";

  for (auto &Function : State.getCallStack()) {
    Out << *Function;
  }

  return Out;
}

} // namespace trace (in seec)

} // namespace seec
