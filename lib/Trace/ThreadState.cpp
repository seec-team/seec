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
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Reverse.hpp"

#include "llvm/Support/raw_ostream.h"

#include <iterator>

namespace seec {

namespace trace {

/// \brief This friend of ThreadState handles dispatching to the various
/// addEvent / removeEvent methods, using SFINAE to avoid attempting to
/// call adders or removers for subservient events.
///
class ThreadMovementDispatcher {
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
};

//------------------------------------------------------------------------------
// ThreadState
//------------------------------------------------------------------------------

ThreadState::ThreadState(ProcessState &Parent,
                         ThreadTrace const &Trace)
: Parent(Parent),
  Trace(Trace),
  m_NextEvent(llvm::make_unique<EventReference>(Trace.events().begin())),
  ProcessTime(Parent.getProcessTime()),
  ThreadTime(0),
  CallStack(),
  m_CompletedFunctions()
{}


//------------------------------------------------------------------------------
// Adding events
//------------------------------------------------------------------------------

void ThreadState::addEvent(EventRecord<EventType::None> const &Ev) {}

// It's OK to find this Event in the middle of a trace, because the trace has
// to be speculatively "ended" before calling exec functions.
void ThreadState::addEvent(EventRecord<EventType::TraceEnd> const &Ev) {}

void ThreadState::addEvent(EventRecord<EventType::FunctionStart> const &Ev) {
  auto Info = llvm::make_unique<FunctionTrace>(Trace.getFunctionTrace(Ev));
  auto const Index = Info->getIndex();
  
  auto const MappedFunction = Parent.getModule().getFunctionIndex(Index);
  assert(MappedFunction && "Couldn't get FunctionIndex");

  auto State = llvm::make_unique<FunctionState>
                (*this,
                 Index,
                 *MappedFunction,
                 Parent.getValueStoreModuleInfo(),
                 std::move(Info));
  assert(State);
  
  CallStack.emplace_back(std::move(State));
  
  ThreadTime = Ev.getThreadTimeEntered();
}

void ThreadState::addEvent(EventRecord<EventType::FunctionEnd> const &Ev) {
  auto const &StartEv = Parent.getTrace()
                              .getEventAtOffset<EventType::FunctionStart>
                                               (Ev.getEventOffsetStart());
  
  auto const Index = StartEv.getFunctionIndex();

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

  m_CompletedFunctions.emplace_back(std::move(CallStack.back()));
  CallStack.pop_back();
  
  ThreadTime = StartEv.getThreadTimeExited();
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
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setActiveInstructionIncomplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(EventRecord<EventType::Instruction> const &Ev) {
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt8> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt16> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt32> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithUInt64> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValueUInt64(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithPtr> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValuePtr(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithFloat> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValueFloat(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithDouble> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  FuncState.setValueDouble(FuncState.getInstruction(Index), Ev.getValue());
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(
      EventRecord<EventType::InstructionWithLongDouble> const &Ev)
{
  auto &FuncState = *(CallStack.back());
  
  auto const Index = Ev.getIndex();
  FuncState.forwardingToInstruction(Index);
  
  uint64_t const Words[2] = { Ev.getValueWord1(), Ev.getValueWord2() };
  auto const Instruction = FuncState.getInstruction(Index);
  auto const Type = Instruction->getType();
  
  if (Type->isX86_FP80Ty()) {
    FuncState.setValueAPFloat(Instruction,
                              llvm::APFloat(llvm::APFloat::x87DoubleExtended(),
                                            llvm::APInt(80, Words)));
  }
  else {
    llvm_unreachable("unhandled long double type");
  }
  
  FuncState.setActiveInstructionComplete(Index);
  
  ++ThreadTime;
}

void ThreadState::addEvent(EventRecord<EventType::StackRestore> const &Ev) {
  auto &FuncState = *(CallStack.back());
  auto const Removed = FuncState.removeAllocas(Ev.getPopCount());

  // Remove allocations for all the cleared allocas.
  for (auto const &Alloca : Removed)
    Parent.Memory.allocationRemove(Alloca.getAddress(), Alloca.getTotalSize());
}

void ThreadState::addEvent(EventRecord<EventType::Alloca> const &Ev) {
  auto MaybeEvRef = Trace.getThreadEventBlockSequence().getReferenceTo(Ev);
  assert(MaybeEvRef && "Malformed event trace");
  auto const &EvRef = *MaybeEvRef;
  
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

  auto &Alloca = Allocas.back();
  Parent.Memory.allocationAdd(Alloca.getAddress(), Alloca.getTotalSize());
}

void ThreadState::addEvent(EventRecord<EventType::Malloc> const &Ev) {
  // Find the preceding InstructionWithPtr event.
  auto MaybeEvRef = Trace.getThreadEventBlockSequence().getReferenceTo(Ev);
  assert(MaybeEvRef && "Malformed event trace");
  auto const &EvRef = *MaybeEvRef;

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
  auto &FuncState = *(CallStack.back());
  FuncState.addByValArea(Ev.getArgument(), Ev.getAddress(), Ev.getSize());
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
  
  auto MaybeEvRef = Trace.getThreadEventBlockSequence().getReferenceTo(Ev);
  assert(MaybeEvRef && "Malformed event trace");
  auto const &EvRef = *MaybeEvRef;
  
  auto ErrRange = rangeAfterIncluding(Trace.events(), EvRef);
  auto ReadError = deserializeRuntimeError(ErrRange);
  assert(ReadError && "Malformed trace file.");
  
  CallStack.back()->addRuntimeError(std::move(ReadError));
}

void ThreadState::addNextEvent() {
  switch ((*m_NextEvent)->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      ThreadMovementDispatcher::addNextEventForwarder<EventType::NAME>         \
        (*this, *m_NextEvent);                                                 \
      break;
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }

  ++*m_NextEvent;
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

void ThreadState::makePreviousInstructionActive(EventRecordBase const &PriorTo)
{
  auto MaybeEvRef =
    Trace.getThreadEventBlockSequence().getReferenceTo(PriorTo);
  assert(MaybeEvRef && "Malformed event trace");
  auto const &EvRef = *MaybeEvRef;
  
  makePreviousInstructionActive(EvRef);
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

void ThreadState::setPreviousViewOfProcessTime(EventRecordBase const &PriorTo)
{
  auto MaybeEvRef =
    Trace.getThreadEventBlockSequence().getReferenceTo(PriorTo);
  assert(MaybeEvRef && "Malformed event trace");
  auto const &EvRef = *MaybeEvRef;
  
  setPreviousViewOfProcessTime(EvRef);
}

void ThreadState::removeEvent(EventRecord<EventType::None> const &Ev) {}

// It's OK to find this Event in the middle of a trace, because the trace has
// to be speculatively "ended" before calling exec functions.
void ThreadState::removeEvent(EventRecord<EventType::TraceEnd> const &Ev) {}

void ThreadState::removeEvent(EventRecord<EventType::FunctionStart> const &Ev) {
  auto const Info = Trace.getFunctionTrace(Ev);
  auto const Index = Info.getIndex();

  assert(CallStack.size() && "Removing FunctionStart with empty CallStack");
  assert(CallStack.back()->getIndex() == Index
         && "Removing FunctionStart does not match currently active function");

  CallStack.pop_back();
  ThreadTime = Info.getThreadTimeEntered() - 1;
}

void ThreadState::removeEvent(EventRecord<EventType::FunctionEnd> const &Ev)
{
  CallStack.emplace_back(std::move(m_CompletedFunctions.back()));
  m_CompletedFunctions.pop_back();
  
  auto &StateRef = *(CallStack.back());
  
  // Restore alloca allocations (reverse order):
  for (auto const &Alloca : seec::reverse(StateRef.getAllocas()))
    Parent.Memory.allocationUnremove(Alloca.getAddress(),
                                     Alloca.getTotalSize());

  // Restore byval areas (reverse order):
  for (auto const &ByVal : seec::reverse(StateRef.getParamByValStates()))
    Parent.Memory.allocationUnremove(ByVal.getArea().address(),
                                     ByVal.getArea().length());

  // Set the thread time to the value that it had prior to this event.
  ThreadTime = StateRef.getTrace().getThreadTimeExited() - 1;
}

void ThreadState::removeEvent(
      EventRecord<EventType::NewProcessTime> const &Ev)
{
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(
      EventRecord<EventType::NewThreadTime> const &Ev)
{
  --ThreadTime;
}

void ThreadState::removeEvent(
      EventRecord<EventType::PreInstruction> const &Ev) {
  makePreviousInstructionActive(Ev);
  --ThreadTime;
}

void ThreadState::removeEvent(EventRecord<EventType::Instruction> const &Ev) {
  makePreviousInstructionActive(Ev);
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
  makePreviousInstructionActive(Ev);                                           \
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
  
  auto const ClearCount = Ev.getPopCount();
  auto const Restored = FuncState.unremoveAllocas(ClearCount);
  
  auto ReverseRestored =
    range(std::reverse_iterator<decltype(Restored.end())>(Restored.end()),
          std::reverse_iterator<decltype(Restored.begin())>(Restored.begin()));
  
  // Restore allocations that have been unremoved. We have to do this in
  // reverse order so that MemoryState retrieves the correct data.
  for (auto const &Alloca : ReverseRestored) {
    Parent.Memory.allocationUnremove(Alloca.getAddress(), Alloca.getTotalSize());
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
  auto MaybeEvRef = Trace.getThreadEventBlockSequence().getReferenceTo(Ev);
  assert(MaybeEvRef && "malformed event trace");
  auto &EvRef = *MaybeEvRef;
  
  auto const MaybeInstrRef = rfind<EventType::InstructionWithPtr>
                                  (rangeBeforeIncluding(Trace.events(), EvRef));
  assert(MaybeInstrRef.assigned() && "Malformed event trace");

  auto const &InstrRef = MaybeInstrRef.get<0>();
  auto const &Instr = InstrRef.get<EventType::InstructionWithPtr>();
  auto const Address = Instr.getValue();

  Parent.unaddMalloc(Address);
  Parent.Memory.allocationUnadd(Address, Ev.getSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(EvRef);
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
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::Realloc> const &Ev) {
  auto const It = Parent.Mallocs.find(Ev.getAddress());
  assert(It != Parent.Mallocs.end());

  It->second.setSize(Ev.getOldSize());
  Parent.Memory.allocationUnresize(Ev.getAddress(),
                                   Ev.getNewSize(),
                                   Ev.getOldSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
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
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::StateUntyped> const &Ev)
{
  Parent.Memory.removeBlock(MemoryArea(Ev.getAddress(), Ev.getDataSize()));
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::StateMemmove> const &Ev)
{
  Parent.Memory.removeCopy(Ev.getSourceAddress(),
                           Ev.getDestinationAddress(),
                           Ev.getSize());
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::StateClear> const &Ev) {
  Parent.Memory.removeClear(MemoryArea(Ev.getAddress(), Ev.getClearSize()));
  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
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
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::FileWrite> const &Ev)
{
  auto const Stream = Parent.getStream(Ev.getFileAddress());
  assert(Stream && "FileWrite with unknown FILE!");
  
  Stream->unwrite(Ev.getDataSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void
ThreadState::removeEvent(EventRecord<EventType::FileWriteFromMemory> const &Ev)
{
  auto const Stream = Parent.getStream(Ev.getFileAddress());
  assert(Stream && "FileWriteFromMemory with unknown FILE!");
  
  Stream->unwrite(Ev.getDataSize());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::FileClose> const &Ev)
{
  auto const Restored = Parent.restoreStream(Ev.getFileAddress());
  assert(Restored && "Failed to restore FILE stream!");

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::DirOpen> const &Ev)
{
  Parent.removeDir(Ev.getDirAddress());

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::DirClose> const &Ev)
{
  auto const &Trace = Parent.getTrace();
  auto const Dirname = Trace.getDataRaw(Ev.getDirnameOffset());
  
  Parent.addDir(DIRState{Ev.getDirAddress(), std::string{Dirname}});

  Parent.ProcessTime = Ev.getProcessTime() - 1;
  setPreviousViewOfProcessTime(Ev);
}

void ThreadState::removeEvent(EventRecord<EventType::RuntimeError> const &Ev) {
  if (!Ev.getIsTopLevel())
    return;
  
  CallStack.back()->removeLastRuntimeError();
}

void ThreadState::removePreviousEvent() {
  --*m_NextEvent;

  switch ((*m_NextEvent)->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      ThreadMovementDispatcher::removePreviousEventForwarder<EventType::NAME>  \
        (*this, *m_NextEvent);                                                 \
      break;
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
}


uint32_t ThreadState::getThreadID() const {
  return Trace.getThreadID();
}

bool ThreadState::isAtStart() const {
  return *m_NextEvent == Trace.events().begin();
}

bool ThreadState::isAtEnd() const {
  return *m_NextEvent == Trace.events().end();
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
  Out << " Thread #" << (State.getTrace().getThreadID() + 1)
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
