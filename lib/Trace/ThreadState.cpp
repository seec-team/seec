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

#include "seec/Preprocessor/MakeMemberFnChecker.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StreamState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/Util/Dispatch.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Reverse.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

/// \brief This friend of ThreadState handles dispatching to the various
/// addEvent / removeEvent methods, using SFINAE to avoid attempting to
/// call adders or removers for subservient events.
///
class ThreadMovementDispatcher {
  SEEC_PP_MAKE_MEMBER_FN_CHECKER(has_readd_event, readdEvent)
  
public:
  /// \brief Adder for non-subservient events (forwards to Thread.addEvent).
  ///
  template<EventType ET>
  static
  typename std::enable_if< !is_subservient<ET>::value >::type
  addNextEventForwarder(ThreadState &Thread, EventReference const &Event)
  {
    assert(!is_function_level<ET>::value || !Thread.getCallStack().empty());
    Thread.addEvent(Event.get<ET>());
  }
  
  /// \brief Added for subservient events (adds nothing).
  ///
  template<EventType ET>
  static
  typename std::enable_if< is_subservient<ET>::value >::type
  addNextEventForwarder(ThreadState &Thread, EventReference const &Event)
  {
    assert(!is_function_level<ET>::value || !Thread.getCallStack().empty());
  }
  
  /// \brief Remover for non-subservient events.
  ///
  template<EventType ET>
  static
  typename std::enable_if< !is_subservient<ET>::value >::type
  removePreviousEventForwarder(ThreadState &Thread, EventReference const &Event)
  {
    Thread.removeEvent(Event.get<ET>());
  }
  
  /// \brief Remover for subservient events (removes nothing).
  ///
  template<EventType ET>
  static
  typename std::enable_if< is_subservient<ET>::value >::type
  removePreviousEventForwarder(ThreadState &Thread, EventReference const &Event)
  {}
  
  /// \brief For events that have a defined readdEvent(), call that.
  ///
  template<EventType ET>
  static
  typename std::enable_if<
    has_readd_event<
      ThreadState,
      void(ThreadState::*)(EventRecord<ET> const &)>::value >::type
  readdOrAddEvent(ThreadState &Thread, EventRecord<ET> const &Ev) {
    Thread.readdEvent(Ev);
  }
  
  /// \brief For events that have no defined readdEvent(), call addEvent().
  ///
  template<EventType ET>
  static
  typename std::enable_if<
    !has_readd_event<
      ThreadState,
      void(ThreadState::*)(EventRecord<ET> const &)>::value >::type
  readdOrAddEvent(ThreadState &Thread, EventRecord<ET> const &Ev) {
    Thread.addEvent(Ev);
  }
  
  /// \brief Restore non-subservient events.
  /// This is used when rewinding a FunctionEnd.
  ///
  template<EventType ET>
  static
  typename std::enable_if< !is_subservient<ET>::value >::type
  restoreEventForwarder(ThreadState &Thread, EventReference const &Event)
  {
    if (is_instruction<ET>::value) {
      Thread.addEvent(Event.get<ET>());
    }
    else if (is_function_level<ET>::value) {
      readdOrAddEvent<ET>(Thread, Event.get<ET>());
    }
  }
  
  /// \brief Restore non-subservient events (does nothing).
  ///
  template<EventType ET>
  static
  typename std::enable_if< is_subservient<ET>::value >::type
  restoreEventForwarder(ThreadState &Thread, EventReference const &Event)
  {}
};

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

  auto State = new FunctionState(*this,
                                 Index,
                                 *MappedFunction,
                                 Parent.getValueStoreModuleInfo(),
                                 Info);
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

  // Remove stack allocations from the memory state.
  auto const &ByVals = CallStack.back()->getParamByValStates();
  for (auto const &ByVal : ByVals)
    Parent.Memory.allocationRemove(ByVal.getArea().address(),
                                   ByVal.getArea().length());

  auto const &Allocas = CallStack.back()->getAllocas();
  for (auto const &Alloca : Allocas)
    Parent.Memory.allocationRemove(Alloca.getAddress(), Alloca.getTotalSize());

  CallStack.pop_back();
  ThreadTime = Info.getThreadTimeExited();
}

void ThreadState::addEvent(EventRecord<EventType::NewProcessTime> const &Ev)
{
  // Update this thread's view of ProcessTime.
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::NewThreadTime> const &Ev)
{
  ++ThreadTime;
}

void ThreadState::addEvent(EventRecord<EventType::PreInstruction> const &Ev) {
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(EventRecord<EventType::Instruction> const &Ev) {
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt8> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt16> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt32> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt64> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithPtr> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithFloat> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithDouble> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithLongDouble> const &Ev)
{
  CallStack.back()->forwardingToInstruction(Ev.getIndex());
  readdEvent(Ev);
  ++ThreadTime;
}

void ThreadState::addEvent(EventRecord<EventType::StackRestore> const &Ev) {
  // Save the pre-allocas (it's OK to move, because they will be cleared).
  auto &FuncState = *(CallStack.back());
  auto const PreAllocas = std::move(FuncState.getAllocas());

  // This clears the allocas and sets them to the post-stackrestore state.
  readdEvent(Ev);

  // Find the first alloca that was not restored.
  auto const &PostAllocas = FuncState.getAllocas();
  auto const Diff = std::mismatch(PostAllocas.begin(), PostAllocas.end(),
                                  PreAllocas.begin());

  // Remove all allocas that were not restored.
  for (auto const &Alloca : seec::range(Diff.second, PreAllocas.end()))
    Parent.Memory.allocationRemove(Alloca.getAddress(), Alloca.getTotalSize());
}

void ThreadState::addEvent(EventRecord<EventType::Alloca> const &Ev) {
  readdEvent(Ev);

  auto &Alloca = CallStack.back()->getAllocas().back();
  Parent.Memory.allocationAdd(Alloca.getAddress(), Alloca.getTotalSize());
}

void ThreadState::addEvent(EventRecord<EventType::Malloc> const &Ev) {
  // Find the preceding InstructionWithPtr event.
  EventReference EvRef(Ev);

  assert(EvRef != Trace.events().begin() && "Malformed event trace");

  auto const MaybeInstrRef = rfind<EventType::InstructionWithPtr>
                                  (rangeBeforeIncluding(Trace.events(), EvRef));

  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &InstrEv = InstrRef.get<EventType::InstructionWithPtr>();

  auto const Address = InstrEv.getValue();
  
  llvm::Instruction const *Allocator = nullptr;
  if (!CallStack.empty())
    Allocator = CallStack.back()->getInstruction(InstrEv.getIndex());
  
  // Update the shared ProcessState.
  Parent.addMalloc(Address, Ev.getSize(), Allocator);
  Parent.Memory.allocationAdd(Address, Ev.getSize());

  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::Free> const &Ev) {
  // Find the original Malloc event.
  auto const Address = Ev.getAddress();
  auto const Size = Parent.Mallocs.find(Address)->second.getSize();

  // Update the shared ProcessState.
  Parent.removeMalloc(Address);
  Parent.Memory.allocationRemove(Address, Size);
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::Realloc> const &Ev) {
  auto const It = Parent.Mallocs.find(Ev.getAddress());
  assert(It != Parent.Mallocs.end());

  It->second.pushAllocator(CallStack.back()->getActiveInstruction());
  It->second.setSize(Ev.getNewSize());
  Parent.Memory.allocationResize(Ev.getAddress(),
                                 Ev.getOldSize(),
                                 Ev.getNewSize());
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateTyped> const &Ev) {
  llvm_unreachable("not yet implemented.");
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void
ThreadState::addEvent(EventRecord<EventType::StateUntypedSmall> const &Ev) {
  auto DataPtr = reinterpret_cast<char const *>(&(Ev.getData()));
  Parent.Memory.addBlock(MappedMemoryBlock(Ev.getAddress(),
                                           Ev.getSize(),
                                           DataPtr));
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateUntyped> const &Ev) {
  auto Data = Parent.getTrace().getData(Ev.getDataOffset(), Ev.getDataSize());
  Parent.Memory.addBlock(MappedMemoryBlock(Ev.getAddress(),
                                           Ev.getDataSize(),
                                           Data.data()));
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateMemmove> const &Ev) {
  Parent.Memory.addCopy(Ev.getSourceAddress(),
                        Ev.getDestinationAddress(),
                        Ev.getSize());
  Parent.ProcessTime = Ev.getProcessTime();
  ProcessTime = Ev.getProcessTime();
}

void ThreadState::addEvent(EventRecord<EventType::StateClear> const &Ev) {
  Parent.Memory.addClear(MemoryArea(Ev.getAddress(), Ev.getClearSize()));
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
  Parent.Memory.allocationAdd(Ev.getAddress(), Ev.getSize());
}

void ThreadState::addEvent(EventRecord<EventType::KnownRegionRemove> const &Ev)
{
  Parent.removeKnownMemory(Ev.getAddress());
  Parent.Memory.allocationRemove(Ev.getAddress(), Ev.getSize());
}

void ThreadState::addEvent(EventRecord<EventType::ByValRegionAdd> const &Ev)
{
  readdEvent(Ev);
  Parent.Memory.allocationAdd(Ev.getAddress(), Ev.getSize());
}

void ThreadState::addEvent(EventRecord<EventType::FileOpen> const &Ev)
{
  auto const &Trace = Parent.getTrace();
  auto const Filename = Trace.getDataRaw(Ev.getFilenameOffset());
  auto const Mode = Trace.getDataRaw(Ev.getModeOffset());
  
  Parent.addStream(StreamState{Ev.getFileAddress(),
                               StreamState::StandardStreamKind::none,
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
  
  if (Ev.getDataSize()) {
    assert(Region.isCompletelyInitialized() &&
           "FileWriteFromMemory with invalid MemoryArea!");
  }
  
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

//------------------------------------------------------------------------------
// readdEvent()
//------------------------------------------------------------------------------

void ThreadState::readdEvent(EventRecord<EventType::NewThreadTime> const &Ev) {}

void ThreadState::readdEvent(EventRecord<EventType::PreInstruction> const &Ev) {
  auto const Index = Ev.getIndex();

  auto &FuncState = *(CallStack.back());
  FuncState.setActiveInstructionIncomplete(Index);
}

void ThreadState::readdEvent(EventRecord<EventType::Instruction> const &Ev) {
  auto const Index = Ev.getIndex();

  auto &FuncState = *(CallStack.back());
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithUInt8> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithUInt16> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithUInt32> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithUInt64> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithPtr> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValuePtr(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithFloat> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValueFloat(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithDouble> const &Ev)
{
  auto const Index = Ev.getIndex();
  auto const Value = Ev.getValue();

  auto &FuncState = *(CallStack.back());
  FuncState.setValueDouble(FuncState.getInstruction(Index), Value);
  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(
      EventRecord<EventType::InstructionWithLongDouble> const &Ev)
{
  auto const Index = Ev.getIndex();
  uint64_t const Words[2] = { Ev.getValueWord1(), Ev.getValueWord2() };

  auto &FuncState = *(CallStack.back());
  auto const Instruction = FuncState.getInstruction(Index);
  auto const Type = Instruction->getType();

  if (Type->isX86_FP80Ty()) {
    FuncState.setValueAPFloat(Instruction,
                              llvm::APFloat(llvm::APFloat::x87DoubleExtended,
                                            llvm::APInt(80, Words)));
  }
  else {
    llvm_unreachable("unhandled long double type");
  }

  FuncState.setActiveInstructionComplete(Index);
}

void ThreadState::readdEvent(EventRecord<EventType::StackRestore> const &Ev) {
  // Clear the current allocas.
  auto &FuncState = *(CallStack.back());
  FuncState.getAllocas().clear();

  // Get all of the StackRestoreAlloca records.
  EventReference EvRef(Ev);
  auto const Allocas = getLeadingBlock<EventType::StackRestoreAlloca>
                                      (rangeAfter(Trace.events(), EvRef));

  // Add the restored allocas.
  for (auto const &RestoreAlloca : Allocas) {
    auto const Offset = RestoreAlloca.getAlloca();
    auto &Alloca = Trace.events().eventAtOffset<EventType::Alloca>(Offset);
    readdEvent(Alloca);
  }
}

void ThreadState::readdEvent(EventRecord<EventType::Alloca> const &Ev) {
  EventReference EvRef(Ev);

  assert(EvRef != Trace.events().begin() && "Malformed event trace");

  // Find the preceding InstructionWithPtr event.
  auto const MaybeInstrRef = rfind<EventType::InstructionWithPtr>(
                                rangeBeforeIncluding(Trace.events(), EvRef));

  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithPtr>();

  // Add Alloca information.
  FunctionState &FuncState = *(CallStack.back());
  auto &Allocas = FuncState.getAllocas();

  Allocas.emplace_back(FuncState,
                       Instr.getIndex(),
                       Instr.getValue(),
                       Ev.getElementSize(),
                       Ev.getElementCount());
}

void ThreadState::readdEvent(EventRecord<EventType::ByValRegionAdd> const &Ev) {
  auto &FuncState = *(CallStack.back());
  FuncState.addByValArea(Ev.getArgument(), Ev.getAddress(), Ev.getSize());
}

void ThreadState::addNextEvent() {
  switch (NextEvent->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      ThreadMovementDispatcher::addNextEventForwarder<EventType::NAME>         \
        (*this, NextEvent);                                                    \
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
  assert(MaybeIndex.hasValue());

  // Set the correct BasicBlocks to be active.
  FuncState.rewindingToInstruction(*MaybeIndex);
  
  if (MaybeRef.get<0>()->getType() != EventType::PreInstruction)
    FuncState.setActiveInstructionComplete(*MaybeIndex);
  else
    FuncState.setActiveInstructionIncomplete(*MaybeIndex);
  
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
                          return Ev.getProcessTime().hasValue();
                        });
  
  if (!MaybeRef.assigned())
    ProcessTime = 0;
  else
    ProcessTime = *(MaybeRef.get<0>()->getProcessTime());
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

  auto State = new FunctionState(*this,
                                 Index,
                                 *MappedFunction,
                                 Parent.getValueStoreModuleInfo(),
                                 Info);
  assert(State);
  
  CallStack.emplace_back(State);
  
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
        ThreadMovementDispatcher::restoreEventForwarder<EventType::NAME>       \
          (*this, RestoreRef);                                                 \
        break;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
  }

  // Restore alloca allocations (reverse order):
  for (auto const &Alloca : seec::reverse(State->getAllocas()))
    Parent.Memory.allocationUnremove(Alloca.getAddress(),
                                     Alloca.getTotalSize());

  // Restore byval areas (reverse order):
  for (auto const &ByVal : seec::reverse(State->getParamByValStates()))
    Parent.Memory.allocationUnremove(ByVal.getArea().address(),
                                     ByVal.getArea().length());

  // Set the thread time to the value that it had prior to this event.
  ThreadTime = Info.getThreadTimeExited() - 1;
}

void ThreadState::removeEvent(
      EventRecord<EventType::NewProcessTime> const &Ev)
{
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(
      EventRecord<EventType::NewThreadTime> const &Ev)
{
  --ThreadTime;
}

void ThreadState::removeEvent(
      EventRecord<EventType::PreInstruction> const &Ev) {
  makePreviousInstructionActive(EventReference(Ev));
  --ThreadTime;
}

void ThreadState::removeEvent(EventRecord<EventType::Instruction> const &Ev) {
  makePreviousInstructionActive(EventReference(Ev));
  --ThreadTime;
}

template<EventType ET>
EventRecord<ET> const *getPreviousSame(ThreadTrace const &Trace,
                                       EventRecord<ET> const &Ev)
{
  auto const Range = rangeBefore(Trace.events(), Ev);
  auto const MaybeRef =
    rfindInFunction(Trace, Range, [&Ev] (EventRecordBase const &Other) {
      return Other.getType() == ET
             && Other.as<ET>().getIndex() == Ev.getIndex();
    });

  if (MaybeRef.template assigned<EventReference>())
    return &(MaybeRef.template get<EventReference>().template get<ET>());

  return nullptr;
}

#define SEEC_IMPLEMENT_REMOVE_INSTRUCTION(TYPE)                                \
void ThreadState::removeEvent(                                                 \
      EventRecord<EventType::InstructionWith##TYPE> const &Ev) {               \
  makePreviousInstructionActive(EventReference(Ev));                           \
  --ThreadTime;                                                                \
}

SEEC_IMPLEMENT_REMOVE_INSTRUCTION(UInt8)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(UInt16)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(UInt32)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(UInt64)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(Ptr)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(Float)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(Double)
SEEC_IMPLEMENT_REMOVE_INSTRUCTION(LongDouble)

#undef SEEC_IMPLEMENT_REMOVE_INSTRUCTION

void ThreadState::removeEvent(EventRecord<EventType::StackRestore> const &Ev) {
  auto &FuncState = *(CallStack.back());
  auto const PostAllocas = std::move(FuncState.getAllocas());

  // Clear the current allocas.
  FuncState.getAllocas().clear();

  // Attempt to find a previous StackRestore in this function.
  auto const MaybePrevious =
    rfindInFunction(Trace, rangeBefore(Trace.events(), Ev),
                    [] (EventRecordBase const &E) {
                      return E.getType() == EventType::StackRestore;
                    });

  if (MaybePrevious.assigned<EventReference>()) {
    auto Events = Trace.events();

    // Add the Allocas that were valid after the previous StackRestore.
    auto &RestoreEv = MaybePrevious.get<EventReference>()
                                   .get<EventType::StackRestore>();
    readdEvent(RestoreEv);

    // Now add all Allocas that occured between the previous StackRestore and
    // the current StackRestore.
    auto PreviousEvRef = MaybePrevious.get<EventReference>();
    EventReference CurrentEvRef(Ev);

    // Iterate through the events, skipping any child functions as we go.
    for (EventReference It(PreviousEvRef); It != CurrentEvRef; ++It) {
      if (It->getType() == EventType::FunctionStart) {
        auto const &StartEv = It.get<EventType::FunctionStart>();
        auto const Info = Trace.getFunctionTrace(StartEv.getRecord());
        It = Events.referenceToOffset(Info.getEventEnd());
        // It will be incremented when we finish this iteration, so the
        // FunctionEnd for this child will (correctly) not be seen.
        assert(It < CurrentEvRef);
      }
      else if (It->getType() == EventType::Alloca) {
        readdEvent(It.get<EventType::Alloca>());
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
        readdEvent(ItEventRef.get<EventType::Alloca>());
      }
    }
  }

  // Find the first alloca that was removed by this StackRestore (but has now
  // been unremoved).
  auto const &PreAllocas = FuncState.getAllocas();
  auto const Diff = std::mismatch(PostAllocas.begin(), PostAllocas.end(),
                                  PreAllocas.begin());

  // Restore allocations that have been unremoved. We have to do this in
  // reverse order so that MemoryState retrieves the correct data.
  typedef decltype(PreAllocas.rbegin()) CRIterTy;

  for (auto const &Alloca : range(CRIterTy(PreAllocas.end()),
                                  CRIterTy(Diff.second)))
  {
    Parent.Memory.allocationUnremove(Alloca.getAddress(),
                                     Alloca.getTotalSize());
  }
}

void ThreadState::removeEvent(EventRecord<EventType::Alloca> const &Ev) {
  // Remove Alloca information.
  auto &Allocas = CallStack.back()->getAllocas();
  assert(!Allocas.empty());

  auto const &Alloca = Allocas.back();
  Parent.Memory.allocationUnadd(Alloca.getAddress(), Alloca.getTotalSize());

  Allocas.pop_back();
}

void ThreadState::removeEvent(EventRecord<EventType::Malloc> const &Ev) {
  // Find the preceding InstructionWithPtr event.
  EventReference EvRef(Ev);
  auto const MaybeInstrRef = rfind<EventType::InstructionWithPtr>
                                  (rangeBeforeIncluding(Trace.events(), EvRef));
  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithPtr>();
  auto const Address = Instr.getValue();

  Parent.unaddMalloc(Address);
  Parent.Memory.allocationUnadd(Address, Ev.getSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::Free> const &Ev) {
  // Information required to recreate the dynamic memory allocation.
  auto const Address = Ev.getAddress();

  // Restore the dynamic memory allocation.
  Parent.unremoveMalloc(Address);

  // Restore the allocation in the memory state.
  auto const Size = Parent.Mallocs.find(Address)->second.getSize();
  Parent.Memory.allocationUnremove(Address, Size);

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::Realloc> const &Ev) {
  auto const It = Parent.Mallocs.find(Ev.getAddress());
  assert(It != Parent.Mallocs.end());

  It->second.setSize(Ev.getOldSize());
  Parent.Memory.allocationUnresize(Ev.getAddress(),
                                   Ev.getNewSize(),
                                   Ev.getOldSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateTyped> const &Ev)
{
  llvm_unreachable("not yet implemented!");
}

void ThreadState::removeEvent(
        EventRecord<EventType::StateUntypedSmall> const &Ev)
{
  Parent.Memory.removeBlock(MemoryArea(Ev.getAddress(), Ev.getSize()));
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateUntyped> const &Ev)
{
  Parent.Memory.removeBlock(MemoryArea(Ev.getAddress(), Ev.getDataSize()));
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateMemmove> const &Ev)
{
  Parent.Memory.removeCopy(Ev.getSourceAddress(),
                           Ev.getDestinationAddress(),
                           Ev.getSize());
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::StateClear> const &Ev) {
  Parent.Memory.removeClear(MemoryArea(Ev.getAddress(), Ev.getClearSize()));
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EventReference(Ev));
}

void ThreadState::removeEvent(EventRecord<EventType::KnownRegionAdd> const &Ev)
{
  Parent.removeKnownMemory(Ev.getAddress());
  Parent.Memory.allocationUnadd(Ev.getAddress(), Ev.getSize());
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
  Parent.Memory.allocationUnremove(Ev.getAddress(), Ev.getSize());
}

void ThreadState::removeEvent(EventRecord<EventType::ByValRegionAdd> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  FuncState.removeByValArea(Ev.getAddress());
  Parent.Memory.allocationUnadd(Ev.getAddress(), Ev.getSize());
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
      ThreadMovementDispatcher::removePreviousEventForwarder<EventType::NAME>  \
        (*this, NextEvent);                                                    \
      break;
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
}


//------------------------------------------------------------------------------
// Memory.
//------------------------------------------------------------------------------

seec::Maybe<MemoryArea>
ThreadState::getContainingMemoryArea(stateptr_ty Address) const {
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

void printComparable(llvm::raw_ostream &Out, ThreadState const &State)
{
  Out << " Thread #" << State.getTrace().getThreadID()
      << " @TT=" << State.getThreadTime()
      << "\n";

  for (auto &Function : State.getCallStack()) {
    printComparable(Out, *Function);
  }
}

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
