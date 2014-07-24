//===- lib/Trace/ThreadState.cpp ------------------------------------------===//
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

#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StreamState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceFormat.hpp"
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
  CallStack()
{}


//------------------------------------------------------------------------------
// Memory state event movement.
//------------------------------------------------------------------------------

void
ThreadState::addMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateTyped> const &State,
  seec::trace::MemoryState &Memory
  )
{
  llvm_unreachable("Not implemented.");
}

void
ThreadState::restoreMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateTyped> const &State,
  MemoryArea const &InArea,
  seec::trace::MemoryState &Memory
  )
{
  llvm_unreachable("Not implemented.");
}

void
ThreadState::addMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateUntypedSmall> const &Ev,
  seec::trace::MemoryState &Memory
  )
{
  EventReference EvRef(Ev);

  auto DataPtr = reinterpret_cast<char const *>(&(Ev.getData()));

  Memory.add(MappedMemoryBlock(Ev.getAddress(), Ev.getSize(), DataPtr),
             EvLoc);
}

void
ThreadState::restoreMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateUntypedSmall> const &Ev,
  MemoryArea const &InArea,
  seec::trace::MemoryState &Memory
  )
{
  EventReference EvRef(Ev);
  
  auto FragmentStartOffset = InArea.start() - Ev.getAddress();
  auto DataPtr = reinterpret_cast<char const *>(&(Ev.getData()));
  DataPtr += FragmentStartOffset;

  Memory.add(MappedMemoryBlock(InArea.start(),
                               InArea.length(),
                               DataPtr),
             EvLoc);
}

void
ThreadState::addMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateUntyped> const &Ev,
  seec::trace::MemoryState &Memory
  )
{
  EventReference EvRef(Ev);
  
  auto Data = Parent.getTrace().getData(Ev.getDataOffset(), Ev.getDataSize());

  Memory.add(MappedMemoryBlock(Ev.getAddress(),
                               Ev.getDataSize(),
                               Data.data()),
             EvLoc);
}

void
ThreadState::restoreMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateUntyped> const &Ev,
  MemoryArea const &InArea,
  seec::trace::MemoryState &Memory
  )
{
  EventReference EvRef(Ev);
  
  auto FragmentStartOffset = InArea.start() - Ev.getAddress();
  auto Data = Parent.getTrace().getData(Ev.getDataOffset(), Ev.getDataSize());
  auto DataPtr = Data.data() + FragmentStartOffset;

  Memory.add(MappedMemoryBlock(InArea.start(),
                               InArea.length(),
                               DataPtr),
             EvLoc);
}

void
ThreadState::addMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateMemmove> const &Ev,
  seec::trace::MemoryState &Memory
  )
{
  EventReference EvRef(Ev);
  
  Parent.Memory.memcpy(Ev.getSourceAddress(),
                       Ev.getDestinationAddress(),
                       Ev.getSize(),
                       EvLoc);
}

void
ThreadState::restoreMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateMemmove> const &Ev,
  seec::trace::MemoryState &Memory
  )
{
  auto const Destination = Ev.getDestinationAddress();
  
  // 1. Create a new, empty MemoryState.
  seec::trace::MemoryState StageMemory;
  
  // 2. Restore the copied events into the new MemoryState.
  EventReference EvRef(Ev);
  while ((++EvRef)->isSubservient()) {
    if (EvRef->getType() == EventType::StateCopied) {
      auto const &CopyEv = EvRef.get<EventType::StateCopied>();
      restoreMemoryState(EventLocation(CopyEv.getStateThreadID(),
                                       CopyEv.getStateOffset()),
                         StageMemory,
                         MemoryArea(CopyEv.getAddress(),
                                    CopyEv.getSize()));
    }
  }
  
  // 3. Move the events in the new MemoryState into the new position.
  StageMemory.memcpy(Ev.getSourceAddress(),
                     Destination,
                     Ev.getSize(),
                     EvLoc);
  
  // 4. Copy the new fragments into Memory.
  auto const &Fragments = StageMemory.getFragmentMap();
  
  auto const End = Fragments.lower_bound(Destination + Ev.getSize());
  
  for (auto It = Fragments.lower_bound(Destination); It != End; ++It) {
    Memory.add(std::move(It->second.getBlock()), EvLoc);
  }
}

void
ThreadState::restoreMemoryState(
  EventLocation const &EvLoc,
  EventRecord<EventType::StateMemmove> const &Ev,
  MemoryArea const &InArea,
  seec::trace::MemoryState &Memory
  )
{
  // 1. Create a new, empty MemoryState.
  seec::trace::MemoryState StageMemory;
  
  // 2. Restore the copied events into the new MemoryState.
  EventReference EvRef(Ev);
  while ((++EvRef)->isSubservient()) {
    if (EvRef->getType() == EventType::StateCopied) {
      auto const &CopyEv = EvRef.get<EventType::StateCopied>();
      
      restoreMemoryState(EventLocation(CopyEv.getStateThreadID(),
                                       CopyEv.getStateOffset()),
                         StageMemory,
                         MemoryArea(CopyEv.getAddress(),
                                    CopyEv.getSize()));
    }
  }
  
  // 3. Move the events in the new MemoryState into the new position.
  auto const Offset = InArea.start() - Ev.getDestinationAddress();
  StageMemory.memcpy(Ev.getSourceAddress() + Offset,
                     InArea.start(),
                     InArea.length(),
                     EvLoc);
  
  // 4. Copy the new fragments into Memory.
  auto const &Fragments = StageMemory.getFragmentMap();
  
  auto const End = Fragments.lower_bound(InArea.end());
  
  for (auto It = Fragments.lower_bound(InArea.address()); It != End; ++It) {
    Memory.add(std::move(It->second.getBlock()), EvLoc);
  }
}

void ThreadState::restoreMemoryState(EventLocation const &Ev,
                                     MemoryState &Memory) {
  if (Ev.getThreadID() != initialDataThreadID()) {
    // Restore the state from the state event.
    auto const StateRef = Parent.getTrace().getEventReference(Ev);
    
    switch (StateRef->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
      case EventType::NAME:                                                    \
        if (is_memory_state<EventType::NAME>::value)                           \
          restoreMemoryState(Ev, StateRef.get<EventType::NAME>(), Memory);     \
        else                                                                   \
          llvm_unreachable("Referenced event is not a state event.");          \
        break;
#include "seec/Trace/Events.def"
      default:
        llvm_unreachable("Reference to unknown event type!");
        break;
    }
  }
  else {
    // Restore the state from a global variable's initial data.
    auto const GVIndex = static_cast<uint32_t>(Ev.getOffset());
    auto const Global = Parent.getModule().getGlobal(GVIndex);
    assert(Global);
    
    auto const ElemTy = Global->getType()->getElementType();
    auto const Size = Parent.getDataLayout().getTypeStoreSize(ElemTy);
    
    auto const &ProcTrace = Parent.getTrace();
    auto const Address = ProcTrace.getGlobalVariableAddress(GVIndex);
    auto const Data = ProcTrace.getGlobalVariableInitialData(GVIndex, Size);
    
    Memory.add(MappedMemoryBlock(Address, Size, Data.data()),
               EventLocation());
  }
}

void ThreadState::restoreMemoryState(EventLocation const &Ev,
                                     MemoryState &Memory,
                                     MemoryArea const &InArea) {
  if (Ev.getThreadID() != initialDataThreadID()) {
    // Restore the state from the state event.
    auto const StateRef = Parent.getTrace().getEventReference(Ev);
    
    switch (StateRef->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
      case EventType::NAME:                                                    \
        if (is_memory_state<EventType::NAME>::value)                           \
          restoreMemoryState(Ev,                                               \
                             StateRef.get<EventType::NAME>(),                  \
                             InArea,                                           \
                             Memory);                                          \
        else                                                                   \
          llvm_unreachable("Referenced event is not a state event.");          \
        break;
#include "seec/Trace/Events.def"
      default:
        llvm_unreachable("Reference to unknown event type!");
        break;
    }
  }
  else {
    // Restore state from a global variable's initial data.
    auto const GVIndex = static_cast<uint32_t>(Ev.getOffset());
    auto const Global = Parent.getModule().getGlobal(GVIndex);
    assert(Global);
    
    auto const ElemTy = Global->getType()->getElementType();
    auto const Size = Parent.getDataLayout().getTypeStoreSize(ElemTy);
    
    auto const &ProcTrace = Parent.getTrace();
    auto const Address = ProcTrace.getGlobalVariableAddress(GVIndex);
    auto const Data = ProcTrace.getGlobalVariableInitialData(GVIndex, Size);
    
    auto const FragmentAddress = InArea.start();
    auto const FragmentSize = InArea.length();
    
    Memory.add(MappedMemoryBlock(FragmentAddress,
                                 FragmentSize,
                                 Data.slice(FragmentAddress - Address,
                                            FragmentSize).data()),
               EventLocation());
  }
}

//------------------------------------------------------------------------------
// Adding events
//------------------------------------------------------------------------------

void ThreadState::addEvent(EventRecord<EventType::None> const &Ev) {}

// It's OK to find this Event in the middle of a trace, because the trace has
// to be speculatively "ended" before calling exec functions.
void ThreadState::addEvent(EventRecord<EventType::TraceEnd> const &Ev) {}

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

void ThreadState::addEvent(EventRecord<EventType::NewProcessTime> const &Ev)
{
  // Update this thread's view of ProcessTime.
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::NewThreadTime> const &Ev)
{
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(EventRecord<EventType::PreInstruction> const &Ev) {
  auto const Index = Ev.getIndex();

  auto &FuncState = *(CallStack.back());
  FuncState.setActiveInstructionIncomplete(Index);
  
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(EventRecord<EventType::Instruction> const &Ev) {
  auto const Index = Ev.getIndex();

  auto &FuncState = *(CallStack.back());
  FuncState.setActiveInstructionComplete(Index);
  
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithSmallValue> const &Ev) {
  auto const Offset = Trace.events().offsetOf(Ev);
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.getRuntimeValue(Index).set(Offset, Value);
  FuncState.setActiveInstructionComplete(Index);
  
  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithValue> const &Ev) {
  auto const Offset = Trace.events().offsetOf(Ev);
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.getRuntimeValue(Index).set(Offset, Value);
  FuncState.setActiveInstructionComplete(Index);

  ThreadTime = Ev.getThreadTime();
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithLargeValue> const &Ev) {
  // TODO
  llvm_unreachable("not yet implemented");
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
  auto const &InstrEv = InstrRef.get<EventType::InstructionWithValue>();

  auto const Address = InstrEv.getValue().UInt64;
  auto const MallocLocation = EventLocation(Trace.getThreadID(),
                                            Trace.events().offsetOf(EvRef));
  
  llvm::Instruction const *Allocator = nullptr;
  if (!CallStack.empty())
    Allocator = CallStack.back()->getInstruction(InstrEv.getIndex());
  
  // Update the shared ProcessState.
  Parent.Mallocs.insert(std::make_pair(Address,
                                       MallocState(Address,
                                                   Ev.getSize(),
                                                   MallocLocation,
                                                   Allocator)));
  
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
  EventLocation EvLoc(Trace.getThreadID(), Trace.events().offsetOf(Ev));
  addMemoryState(EvLoc, Ev, Parent.Memory);
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void
ThreadState::addEvent(EventRecord<EventType::StateUntypedSmall> const &Ev) {
  EventLocation EvLoc(Trace.getThreadID(), Trace.events().offsetOf(Ev));
  addMemoryState(EvLoc, Ev, Parent.Memory);
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateUntyped> const &Ev) {
  EventLocation EvLoc(Trace.getThreadID(), Trace.events().offsetOf(Ev));
  addMemoryState(EvLoc, Ev, Parent.Memory);
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateMemmove> const &Ev) {
  EventLocation EvLoc(Trace.getThreadID(), Trace.events().offsetOf(Ev));
  addMemoryState(EvLoc, Ev, Parent.Memory);
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateClear> const &Ev) {
  Parent.Memory.clear(MemoryArea(Ev.getAddress(), Ev.getClearSize()));
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::KnownRegionAdd> const &Ev) {
  auto const Access =
    Ev.getReadable() ? (Ev.getWritable() ? MemoryPermission::ReadWrite
                                         : MemoryPermission::ReadOnly)
                     : (Ev.getWritable() ? MemoryPermission::WriteOnly
                                         : MemoryPermission::None);
  
  Parent.addKnownMemory(Ev.getAddress(), Ev.getSize(), Access);
}

void ThreadState::addEvent(EventRecord<EventType::KnownRegionRemove> const &Ev)
{
  Parent.removeKnownMemory(Ev.getAddress());
}

void ThreadState::addEvent(EventRecord<EventType::ByValRegionAdd> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  FuncState.addByValArea(Ev.getArgument(), Ev.getAddress(), Ev.getSize());
}

void ThreadState::addEvent(EventRecord<EventType::FileOpen> const &Ev)
{
  auto const &Trace = Parent.getTrace();
  auto const Filename = Trace.getDataRaw(Ev.getFilenameOffset());
  auto const Mode = Trace.getDataRaw(Ev.getModeOffset());
  
  Parent.addStream(StreamState{Ev.getFileAddress(),
                               std::string{Filename},
                               std::string{Mode}});

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::FileWrite> const &Ev)
{
  auto const Stream = Parent.getStream(Ev.getFileAddress());
  assert(Stream && "FileWrite with unknown FILE!");
  
  Stream->write(Parent.getTrace().getData(Ev.getDataOffset(),
                                          Ev.getDataSize()));

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void
ThreadState::addEvent(EventRecord<EventType::FileWriteFromMemory> const &Ev)
{
  auto const Stream = Parent.getStream(Ev.getFileAddress());
  assert(Stream && "FileWriteFromMemory with unknown FILE!");
  
  auto const Region = Parent.Memory.getRegion(MemoryArea(Ev.getDataAddress(),
                                                         Ev.getDataSize()));
  
  assert(Region.isCompletelyInitialized() &&
         "FileWriteFromMemory with invalid MemoryArea!");
  
  Stream->write(Region.getByteValues());

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::FileClose> const &Ev)
{
  Parent.closeStream(Ev.getFileAddress());

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::DirOpen> const &Ev)
{
  auto const &Trace = Parent.getTrace();
  auto const Dirname = Trace.getDataRaw(Ev.getDirnameOffset());
  
  Parent.addDir(DIRState{Ev.getDirAddress(), std::string{Dirname}});

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::DirClose> const &Ev)
{
  Parent.removeDir(Ev.getDirAddress());

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::RuntimeError> const &Ev) {
  if (!Ev.getIsTopLevel())
    return;
  
  auto ErrRange = rangeAfterIncluding(Trace.events(), Ev);
  auto ReadError = deserializeRuntimeError(ErrRange);
  assert(ReadError.first && "Malformed trace file.");
  
  CallStack.back()->addRuntimeError(std::move(ReadError.first));
}

void ThreadState::addNextEvent() {
  switch (NextEvent->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      assert(!is_function_level<EventType::NAME>::value || !CallStack.empty());\
      if (!is_subservient<EventType::NAME>::value) {                           \
        addEvent(NextEvent.get<EventType::NAME>());                            \
      }                                                                        \
      break;
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }

  ++NextEvent;
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

  if (MaybeRef.get<0>()->getType() != EventType::PreInstruction)
    FuncState.setActiveInstructionComplete(MaybeIndex.get<0>());
  else
    FuncState.setActiveInstructionIncomplete(MaybeIndex.get<0>());
  
  // Find all runtime errors attached to the previous instruction, and make
  // them active.
  auto ErrorSearchRange = EventRange(MaybeRef.get<0>(), PriorTo);
  
  for (auto Ev : ErrorSearchRange) {
    if (Ev.getType() != EventType::RuntimeError)
      continue;
    
    addEvent(Ev.as<EventType::RuntimeError>());
  }
}

void ThreadState::setPreviousViewOfProcessTime(EventReference PriorTo) {
  // Find the previous event that sets the process time, if there is one.
  auto MaybeRef = rfind(rangeBefore(Trace.events(), PriorTo),
                        [](EventRecordBase const &Ev) -> bool {
                          return Ev.getProcessTime().assigned();
                        });
  
  if (!MaybeRef.assigned())
    ProcessTime = 0;
  else
    ProcessTime = MaybeRef.get<0>()->getProcessTime().get<0>();
}

void ThreadState::removeEvent(EventRecord<EventType::None> const &Ev) {}

// It's OK to find this Event in the middle of a trace, because the trace has
// to be speculatively "ended" before calling exec functions.
void ThreadState::removeEvent(EventRecord<EventType::TraceEnd> const &Ev) {}

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
      
      // Set the thread time to the FunctionEnd event's thread time.
      ThreadTime = Child.getThreadTimeExited();
      
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
      EventRecord<EventType::NewProcessTime> const &Ev)
{
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(
      EventRecord<EventType::NewThreadTime> const &Ev)
{
  ThreadTime = Ev.getThreadTime() - 1;
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
  setPreviousViewOfProcessTime(EventReference(Ev));
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
  auto const &InstrEv = InstrRef.get<EventType::InstructionWithValue>();

  // Information required to recreate the dynamic memory allocation.
  auto const Address = InstrEv.getValue().UInt64;

  EventLocation MallocLocation(MallocThreadID, MallocOffset);
  
  // Attempt to find the llvm::Instruction that allocated this memory.
  llvm::Instruction const *Allocator = nullptr;
  
  auto const MaybeFuncStartRef = rfind<EventType::FunctionStart>
                                      (rangeBefore(MallocThread.events(),
                                                   InstrRef));
  if (MaybeFuncStartRef.assigned(0)) {
    auto const &FuncStartRef = MaybeFuncStartRef.get<0>();
    auto const &FuncStartEv = FuncStartRef.get<EventType::FunctionStart>();
    
    auto const FnInfo = Trace.getFunctionTrace(FuncStartEv.getRecord());
    auto const FnIndex = FnInfo.getIndex();
    
    auto const MappedFunction = Parent.getModule().getFunctionIndex(FnIndex);
    if (MappedFunction)
      Allocator = MappedFunction->getInstruction(InstrEv.getIndex());
  }

  // Update the shared ProcessState.
  Parent.Mallocs.insert(std::make_pair(Address,
                                       MallocState(Address,
                                                   MallocEv.getSize(),
                                                   MallocLocation,
                                                   Allocator)));

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

bool ThreadState::removeEventIfOverwrite(EventReference EvRef) {
  switch (EvRef->getType()) {
    case EventType::StateOverwrite:
      removeEvent(EvRef.get<EventType::StateOverwrite>());
      return true;
    case EventType::StateOverwriteFragment:
      removeEvent(EvRef.get<EventType::StateOverwriteFragment>());
      return true;
    case EventType::StateOverwriteFragmentTrimmedRight:
      removeEvent(EvRef.get<EventType::StateOverwriteFragmentTrimmedRight>());
      return true;
    case EventType::StateOverwriteFragmentTrimmedLeft:
      removeEvent(EvRef.get<EventType::StateOverwriteFragmentTrimmedLeft>());
      return true;
    case EventType::StateOverwriteFragmentSplit:
      removeEvent(EvRef.get<EventType::StateOverwriteFragmentSplit>());
      return true;
    default:
      return false;
  }
}

void ThreadState::removeEvent(EventRecord<EventType::StateTyped> const &Ev) {
  // Clear this state.
  // TODO.

  // Restore any overwritten states.
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;
    assert(Trace.events().contains(EvRef) && "Malformed trace!");
    auto Removed = removeEventIfOverwrite(EvRef);
    assert(Removed && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
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
    assert(Trace.events().contains(EvRef) && "Malformed trace!");
    auto Removed = removeEventIfOverwrite(EvRef);
    assert(Removed && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateUntyped> const &Ev) {
  // Clear this state.
  Parent.Memory.clear(MemoryArea(Ev.getAddress(), Ev.getDataSize()));

  // Restore any overwritten states.
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;
    assert(Trace.events().contains(EvRef) && "Malformed trace!");
    auto Removed = removeEventIfOverwrite(EvRef);
    assert(Removed && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateMemmove> const &Ev) {
  // Clear this state.
  Parent.Memory.clear(MemoryArea(Ev.getDestinationAddress(), Ev.getSize()));

  // Restore any overwritten states.
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;
    assert(Trace.events().contains(EvRef) && "Malformed trace!");
    auto Removed = removeEventIfOverwrite(EvRef);
    assert(Removed && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateClear> const &Ev) {
  auto const Overwritten = Ev.getOverwritten();

  EventReference EvRef(Ev);
  for (auto i = Overwritten; i != 0; --i) {
    ++EvRef;
    assert(Trace.events().contains(EvRef) && "Malformed trace!");
    auto Removed = removeEventIfOverwrite(EvRef);
    assert(Removed && "Malformed trace!");
  }

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(
  EventRecord<EventType::StateOverwrite> const &Ev)
{
  restoreMemoryState(EventLocation(Ev.getStateThreadID(), Ev.getStateOffset()),
                     Parent.Memory);
}

void ThreadState::removeEvent(
  EventRecord<EventType::StateOverwriteFragment> const &Ev)
{
  restoreMemoryState(EventLocation(Ev.getStateThreadID(), Ev.getStateOffset()),
                     Parent.Memory,
                     MemoryArea(Ev.getFragmentAddress(), Ev.getFragmentSize()));
}

void ThreadState::removeEvent(
  EventRecord<EventType::StateOverwriteFragmentTrimmedRight> const &Ev) {
  Parent.Memory.untrimRightSide(Ev.getAddressOfBlock(), Ev.getAmountTrimmed());
}

void ThreadState::removeEvent(
  EventRecord<EventType::StateOverwriteFragmentTrimmedLeft> const &Ev) {
  Parent.Memory.untrimLeftSide(Ev.getTrimmedAddressOfBlock(),
                               Ev.getPreviousAddressOfBlock());
}

void ThreadState::removeEvent(
  EventRecord<EventType::StateOverwriteFragmentSplit> const &Ev) {
  Parent.Memory.unsplit(Ev.getAddressOfLeftBlock(),
                        Ev.getAddressOfRightBlock());
}

void ThreadState::removeEvent(EventRecord<EventType::KnownRegionAdd> const &Ev)
{
  Parent.removeKnownMemory(Ev.getAddress());
}

void
ThreadState::removeEvent(EventRecord<EventType::KnownRegionRemove> const &Ev)
{
  auto const Access =
    Ev.getReadable() ? (Ev.getWritable() ? MemoryPermission::ReadWrite
                                         : MemoryPermission::ReadOnly)
                     : (Ev.getWritable() ? MemoryPermission::WriteOnly
                                         : MemoryPermission::None);
  
  Parent.addKnownMemory(Ev.getAddress(), Ev.getSize(), Access);
}

void ThreadState::removeEvent(EventRecord<EventType::ByValRegionAdd> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  FuncState.removeByValArea(Ev.getAddress());
}

void ThreadState::removeEvent(EventRecord<EventType::FileOpen> const &Ev)
{
  Parent.removeStream(Ev.getFileAddress());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::FileWrite> const &Ev)
{
  auto const Stream = Parent.getStream(Ev.getFileAddress());
  assert(Stream && "FileWrite with unknown FILE!");
  
  Stream->unwrite(Ev.getDataSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void
ThreadState::removeEvent(EventRecord<EventType::FileWriteFromMemory> const &Ev)
{
  auto const Stream = Parent.getStream(Ev.getFileAddress());
  assert(Stream && "FileWriteFromMemory with unknown FILE!");
  
  Stream->unwrite(Ev.getDataSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::FileClose> const &Ev)
{
  auto const Restored = Parent.restoreStream(Ev.getFileAddress());
  assert(Restored && "Failed to restore FILE stream!");

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::DirOpen> const &Ev)
{
  Parent.removeDir(Ev.getDirAddress());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::DirClose> const &Ev)
{
  auto const &Trace = Parent.getTrace();
  auto const Dirname = Trace.getDataRaw(Ev.getDirnameOffset());
  
  Parent.addDir(DIRState{Ev.getDirAddress(), std::string{Dirname}});

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::RuntimeError> const &Ev) {
  if (!Ev.getIsTopLevel())
    return;
  
  CallStack.back()->removeLastRuntimeError();
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


//------------------------------------------------------------------------------
// Memory.
//------------------------------------------------------------------------------

seec::Maybe<MemoryArea>
ThreadState::getContainingMemoryArea(uintptr_t Address) const {
  seec::Maybe<MemoryArea> Area;
  
  for (auto const &FunctionStatePtr : CallStack) {
    Area = FunctionStatePtr->getContainingMemoryArea(Address);
    if (Area.assigned())
      break;
  }
  
  return Area;
}


//------------------------------------------------------------------------------
// Printing.
//------------------------------------------------------------------------------

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              ThreadState const &State) {
  Out << " Thread #" << State.getTrace().getThreadID()
      << " @TT=" << State.getThreadTime()
      << "\n";

  for (auto &Function : State.getCallStack()) {
    Out << *Function;
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
