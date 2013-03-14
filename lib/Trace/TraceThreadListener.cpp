//===- lib/Trace/TraceThreadListener.cpp ----------------------------------===//
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

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define _POSIX_SOURCE
#include <signal.h>

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// Helper methods
//------------------------------------------------------------------------------

void TraceThreadListener::synchronizeProcessTime() {
  auto RealProcessTime = ProcessListener.getTime();
  
  if (RealProcessTime != ProcessTime) {
    ProcessTime = RealProcessTime;
    
    EventsOut.write<EventType::NewProcessTime>(ProcessTime);
  }
}

void TraceThreadListener::checkSignals() {
  static std::mutex CheckSignalsMutex;
  
  int Success = 0;
  int Caught = 0;
  sigset_t Empty;
  sigset_t Pending;
  
  Success = sigemptyset(&Empty);
  assert(Success == 0 && "sigemptyset() failed.");
  
  {
    std::lock_guard<std::mutex> Lock (CheckSignalsMutex);
    
    Success = sigpending(&Pending);
    assert(Success == 0 && "sigpending() failed.");
    
    if (memcmp(&Empty, &Pending, sizeof(sigset_t)) == 0)
      return;
    
    // There is a pending signal.
    Success = sigwait(&Pending, &Caught);
    assert(Success == 0 && "sigwait() failed.");
  }
  
  // TODO: Write the signal into the trace.
  // Describe signal using strsignal().
  
  // Perform a coordinated exit (traces will be finalized during destruction).
  getSupportSynchronizedExit().getSynchronizedExit().exit(EXIT_FAILURE);
}


//------------------------------------------------------------------------------
// Dynamic memory
//------------------------------------------------------------------------------

void TraceThreadListener::recordMalloc(uintptr_t Address, std::size_t Size) {
  ProcessTime = getCIProcessTime();
  
  auto Offset = EventsOut.write<EventType::Malloc>(Size, ProcessTime);

  // update dynamic allocation lookup
  ProcessListener.setCurrentDynamicMemoryAllocation(Address,
                                                    ThreadID,
                                                    Offset,
                                                    Size);
}

DynamicAllocation TraceThreadListener::recordFree(uintptr_t Address) {
  auto MaybeMalloc = ProcessListener.getCurrentDynamicMemoryAllocation(Address);

  // If the allocation didn't exist it should have been caught in preCfree.
  assert(MaybeMalloc.assigned() && "recordFree with unassigned address.");
  
  auto Malloc = MaybeMalloc.get<0>();
  
  // Get new process time and update this thread's view of process time.
  ProcessTime = getCIProcessTime();

  // Write Free event.
  EventsOut.write<EventType::Free>(Malloc.thread(),
                                   Malloc.offset(),
                                   ProcessTime);

  // Update dynamic allocation lookup.
  ProcessListener.removeCurrentDynamicMemoryAllocation(Address);
  
  return Malloc;
}

void TraceThreadListener::recordFreeAndClear(uintptr_t Address) {
  auto Malloc = recordFree(Address);
  
  // Clear the state of the freed area.
  recordStateClear(Malloc.address(), Malloc.size());
}


//------------------------------------------------------------------------------
// Memory states
//------------------------------------------------------------------------------

void
TraceThreadListener::writeStateOverwritten(OverwrittenMemoryInfo const &Info)
{
  for (auto const &Overwrite : Info.overwrites()) {
    auto &Event = Overwrite.getStateEvent();
    auto &OldArea = Overwrite.getOldArea();
    auto &NewArea = Overwrite.getNewArea();
    
    switch (Overwrite.getType()) {
      case StateOverwrite::OverwriteType::ReplaceState:
        EventsOut.write<EventType::StateOverwrite>
                       (Event.getThreadID(),
                        Event.getOffset());
        break;
      
      case StateOverwrite::OverwriteType::ReplaceFragment:
        EventsOut.write<EventType::StateOverwriteFragment>
                       (Event.getThreadID(),
                        Event.getOffset(),
                        NewArea.address(),
                        NewArea.length());
        break;
      
      case StateOverwrite::OverwriteType::TrimFragmentRight:
        EventsOut.write<EventType::StateOverwriteFragmentTrimmedRight>
                       (OldArea.address(),
                        OldArea.end() - NewArea.start());
        break;
      
      case StateOverwrite::OverwriteType::TrimFragmentLeft:
        EventsOut.write<EventType::StateOverwriteFragmentTrimmedLeft>
                       (NewArea.end(),
                        OldArea.start());
        break;
      
      case StateOverwrite::OverwriteType::SplitFragment:
        EventsOut.write<EventType::StateOverwriteFragmentSplit>
                       (OldArea.address(),
                        NewArea.end());
        break;
    }
  }
}

void TraceThreadListener::recordUntypedState(char const *Data,
                                             std::size_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  uintptr_t Address = reinterpret_cast<uintptr_t>(Data);

  ProcessTime = getCIProcessTime();
  
  // Update the process' memory trace with the new state, and find the states
  // that were overwritten.
  auto MemoryState = ProcessListener.getTraceMemoryStateAccessor();
  auto OverwrittenInfo = MemoryState->add(Address,
                                          Size,
                                          ThreadID,
                                          EventsOut.offset(),
                                          ProcessTime);
  
  if (Size <= EventRecord<EventType::StateUntypedSmall>::sizeofData()) {
    EventRecord<EventType::StateUntypedSmall>::typeofData DataStore;
    char *DataStorePtr = reinterpret_cast<char *>(&DataStore);
    memcpy(DataStorePtr, Data, Size);
    
    // Write the state information to the trace.
    EventsOut.write<EventType::StateUntypedSmall>(
      static_cast<uint8_t>(Size),
      static_cast<uint32_t>(OverwrittenInfo.overwrites().size()),
      Address,
      ProcessTime,
      DataStore);
  }
  else {
    auto DataOffset = ProcessListener.recordData(Data, Size);

    // Write the state information to the trace.
    EventsOut.write<EventType::StateUntyped>(
      static_cast<uint32_t>(OverwrittenInfo.overwrites().size()),
      Address,
      ProcessTime,
      DataOffset,
      Size);
  }
  
  writeStateOverwritten(OverwrittenInfo);
  
  // TODO: add non-local change records for all appropriate functions
}

void TraceThreadListener::recordTypedState(void const *Data,
                                           std::size_t Size,
                                           offset_uint Value){
  recordUntypedState(reinterpret_cast<char const *>(Data), Size);
}

void TraceThreadListener::recordStateClear(uintptr_t Address,
                                           std::size_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  ProcessTime = getCIProcessTime();
  
  auto MemoryState = ProcessListener.getTraceMemoryStateAccessor();
  auto OverwrittenInfo = MemoryState->clear(Address, Size);
  
  EventsOut.write<EventType::StateClear>(
    static_cast<uint32_t>(OverwrittenInfo.overwrites().size()),
    Address,
    ProcessTime,
    Size);
  
  writeStateOverwritten(OverwrittenInfo);
}

void TraceThreadListener::recordMemset() {
  llvm_unreachable("recordMemset unimplemented");
}

void TraceThreadListener::recordMemmove(uintptr_t Source,
                                        uintptr_t Destination,
                                        std::size_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  ProcessTime = getCIProcessTime();
  
  auto const MemoryState = ProcessListener.getTraceMemoryStateAccessor();
  auto const MoveInfo = MemoryState->memmove(Source,
                                             Destination,
                                             Size,
                                             EventLocation(ThreadID,
                                                           EventsOut.offset()),
                                             ProcessTime);
  
  auto const &OverwrittenInfo = MoveInfo.first;
  
  auto const OverwrittenCount =
                    static_cast<uint32_t>(OverwrittenInfo.overwrites().size());
  
  EventsOut.write<EventType::StateMemmove>(OverwrittenCount,
                                           ProcessTime,
                                           Source,
                                           Destination,
                                           Size);
  
  // Write events describing the overwritten states.
  writeStateOverwritten(OverwrittenInfo);
  
  // Write events describing the copied states.
  for (auto const &Copy : MoveInfo.second) {
    EventsOut.write<EventType::StateCopied>(Copy.getEvent().getThreadID(),
                                            Copy.getEvent().getOffset(),
                                            Copy.getArea().start(),
                                            Copy.getArea().length());
  }
}

void TraceThreadListener::addKnownMemoryRegion(uintptr_t Address,
                                               std::size_t Length,
                                               seec::MemoryPermission Access)
{
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  ProcessListener.addKnownMemoryRegion(Address, Length, Access);
  
  auto const Readable = (Access == seec::MemoryPermission::ReadOnly) ||
                        (Access == seec::MemoryPermission::ReadWrite);
  
  auto const Writable = (Access == seec::MemoryPermission::WriteOnly) ||
                        (Access == seec::MemoryPermission::ReadWrite);
  
  EventsOut.write<EventType::KnownRegionAdd>
                 (Address, Length, Readable, Writable);
}

bool TraceThreadListener::removeKnownMemoryRegion(uintptr_t Address)
{
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  auto const &KnownMemory = ProcessListener.getKnownMemory();
  auto const It = KnownMemory.find(Address);
  
  if (It == KnownMemory.end())
    return false;
  
  auto const KeyAddress = It->Begin;
  auto const Length = (It->End - It->Begin) + 1; // Range is inclusive.
  auto const Access = It->Value;
  
  auto const Result = ProcessListener.removeKnownMemoryRegion(Address);
  
  if (!Result)
    return false;
  
  auto const Readable = (Access == seec::MemoryPermission::ReadOnly) ||
                        (Access == seec::MemoryPermission::ReadWrite);
  
  auto const Writable = (Access == seec::MemoryPermission::WriteOnly) ||
                        (Access == seec::MemoryPermission::ReadWrite);
  
  EventsOut.write<EventType::KnownRegionRemove>
                 (KeyAddress, Length, Readable, Writable);
  
  return Result;
}


//------------------------------------------------------------------------------
// Constructor and destructor.
//------------------------------------------------------------------------------

TraceThreadListener::TraceThreadListener(TraceProcessListener &ProcessListener,
                                         OutputStreamAllocator &StreamAllocator)
: seec::trace::CallDetector<TraceThreadListener>
                           (ProcessListener.getDetectCallsLookup()),
  ProcessListener(ProcessListener),
  SupportSyncExit(ProcessListener.syncExit()),
  ThreadID(ProcessListener.registerThreadListener(this)),
  StreamAllocator(StreamAllocator),
  OutputEnabled(false),
  EventsOut(),
  Time(0),
  ProcessTime(0),
  RecordedFunctions(),
  RecordedTopLevelFunctions(),
  FunctionStack(),
  FunctionStackMutex(),
  ActiveFunction(nullptr),
  GlobalMemoryLock(),
  DynamicMemoryLock(),
  StreamsLock()
{
  traceOpen();
  
  // Setup signal handling for this thread.
  int Success = 0;
  sigset_t BlockedSignals;
  
  Success |= sigfillset(&BlockedSignals);
  
  // Remove signals that cause undefined behaviour when blocked.
  Success |= sigdelset(&BlockedSignals, SIGBUS);
  Success |= sigdelset(&BlockedSignals, SIGFPE);
  Success |= sigdelset(&BlockedSignals, SIGILL);
  Success |= sigdelset(&BlockedSignals, SIGSEGV);
  
  Success |= sigprocmask(SIG_BLOCK, &BlockedSignals, NULL);
  assert(Success == 0 && "Failed to setup signal blocking.");
}

TraceThreadListener::~TraceThreadListener()
{
  traceWrite();
  traceFlush();
  traceClose();
  
  ProcessListener.deregisterThreadListener(ThreadID);
}


//------------------------------------------------------------------------------
// Trace writing control.
//------------------------------------------------------------------------------

void TraceThreadListener::traceWrite()
{
  if (!OutputEnabled)
    return;
  
  // Terminate the event stream.
  EventsOut.write<EventType::TraceEnd>(0);
  
  // Write the trace information.
  auto TraceOut = StreamAllocator.getThreadStream(ThreadID,
                                                  ThreadSegment::Trace);
  assert(TraceOut && "Couldn't get thread trace stream.");
  
  offset_uint Written = 0;

  // Calculate the offset position of the first (top-level functions) list.
  offset_uint ListOffset = getNewFunctionRecordOffset();

  // Write offset of top-level function list.
  Written += writeBinary(*TraceOut, ListOffset);
  
  // Calculate offset of the next function list.
  ListOffset += getWriteBinarySize(RecordedTopLevelFunctions);

  // Write function information.
  for (auto const &Function: RecordedFunctions) {
    Written += writeBinary(*TraceOut, Function->getIndex());
    Written += writeBinary(*TraceOut, Function->getEventOffsetStart());
    Written += writeBinary(*TraceOut, Function->getEventOffsetEnd());
    Written += writeBinary(*TraceOut, Function->getThreadTimeEntered());
    Written += writeBinary(*TraceOut, Function->getThreadTimeExited());
    
    // Write offset of the child function list.
    Written += writeBinary(*TraceOut, ListOffset);
    
    // Calculate offset of the next function list.
    ListOffset += getWriteBinarySize(Function->getChildren());
  }

  // Write the top-level function list.
  Written += writeBinary(*TraceOut, RecordedTopLevelFunctions);

  // Write the child lists.
  for (auto const &Function: RecordedFunctions) {
    Written += writeBinary(*TraceOut, Function->getChildren());
  }
}

void TraceThreadListener::traceFlush()
{
  EventsOut.flush();
}

void TraceThreadListener::traceClose()
{
  EventsOut.close();
}

void TraceThreadListener::traceOpen()
{
  EventsOut.open(
    StreamAllocator.getThreadStream(ThreadID,
                                    ThreadSegment::Events,
                                    llvm::raw_fd_ostream::F_Append));
  
  OutputEnabled = true;
}


//------------------------------------------------------------------------------
// Mutators
//------------------------------------------------------------------------------

static void writeError(EventWriter &EventsOut,
                       seec::runtime_errors::RunError const &Error,
                       bool IsTopLevel)
{
  uint16_t Type = static_cast<uint16_t>(Error.type());
  auto const &Args = Error.args();
  auto const &Additional = Error.additional();
  
  EventsOut.write<EventType::RuntimeError>
                 (Type,
                  static_cast<uint8_t>(Args.size()),
                  static_cast<uint8_t>(Additional.size()),
                  static_cast<uint8_t>(IsTopLevel));
  
  for (auto const &Argument : Args) {
    EventsOut.write<EventType::RuntimeErrorArgument>(
      static_cast<uint8_t>(Argument->type()),
      Argument->data());
  }
  
  for (auto const &AdditionalError : Additional) {
    writeError(EventsOut, *AdditionalError, false);
  }
}

void
TraceThreadListener
::handleRunError(seec::runtime_errors::RunError const &Error,
                 RunErrorSeverity Severity,
                 seec::util::Maybe<uint32_t> PreInstructionIndex)
{
  // PreInstruction event precedes the RuntimeError
  if (PreInstructionIndex.assigned()) {
    EventsOut.write<EventType::PreInstruction>(PreInstructionIndex.get<0>(),
                                               ++Time);
  }
  
  writeError(EventsOut, Error, true);
  
  switch (Severity) {
    case RunErrorSeverity::Warning:
      break;
    case RunErrorSeverity::Fatal:
      llvm::errs() << "\nSeeC: Fatal runtime error detected!"
                      " Replay trace for more details.\n";
      
      // Shut down the tracing.
      SupportSyncExit.getSynchronizedExit().exit(EXIT_FAILURE);
      
      break;
  }
}


} // namespace trace (in seec)

} // namespace seec
