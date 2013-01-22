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

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// Helper methods
//------------------------------------------------------------------------------

void TraceThreadListener::writeTrace() {
  EventsOut.write<EventType::TraceEnd>(0);
  
  offset_uint Written = 0;

  // Calculate the offset position of the first (top-level functions) list.
  offset_uint ListOffset = getNewFunctionRecordOffset();

  // Write offset of top-level function list.
  Written += writeBinary(*TraceOut, ListOffset);
  
  // Calculate offset of the next function list.
  ListOffset += getWriteBinarySize(RecordedTopLevelFunctions);

  // write functions
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

void TraceThreadListener::synchronizeProcessTime() {
  auto RealProcessTime = ProcessListener.getTime();
  
  if (RealProcessTime != ProcessTime) {
    ProcessTime = RealProcessTime;
    
    EventsOut.write<EventType::NewProcessTime>(ProcessTime);
  }
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


//------------------------------------------------------------------------------
// Mutators
//------------------------------------------------------------------------------

void TraceThreadListener::handleRunError(
        std::unique_ptr<seec::runtime_errors::RunError> Error,
        RunErrorSeverity Severity,
        seec::util::Maybe<uint32_t> PreInstructionIndex) {
  // PreInstruction event precedes the RuntimeError
  if (PreInstructionIndex.assigned()) {
    EventsOut.write<EventType::PreInstruction>(PreInstructionIndex.get<0>(),
                                               ++Time);
  }
  
  uint16_t Type = static_cast<uint16_t>(Error->type());
  auto const &Args = Error->args();
  
  EventsOut.write<EventType::RuntimeError>(Type,
                                           static_cast<uint32_t>(Args.size()));
  
  for (auto const &Argument : Args) {
    EventsOut.write<EventType::RuntimeErrorArgument>(
      static_cast<uint8_t>(Argument->type()),
      Argument->data());
  }
  
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
