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

  // calculate the offset position of the first (top-level functions) list
  offset_uint ListOffset = getNewFunctionRecordOffset();

  // write offset of top-level function list
  Written += writeBinary(*TraceOut, ListOffset);

  ListOffset += getWriteBinarySize(RecordedTopLevelFunctions);

  // write functions
  for (auto const &Function: RecordedFunctions) {
    Written += writeBinary(*TraceOut, Function->getIndex());
    Written += writeBinary(*TraceOut, Function->getEventOffsetStart());
    Written += writeBinary(*TraceOut, Function->getEventOffsetEnd());
    Written += writeBinary(*TraceOut, Function->getThreadTimeEntered());
    Written += writeBinary(*TraceOut, Function->getThreadTimeExited());
    // child list offset
    Written += writeBinary(*TraceOut, ListOffset);
    ListOffset += getWriteBinarySize(Function->getChildren());
    // non-local change list offset
    Written += writeBinary(*TraceOut, ListOffset);
    ListOffset += getWriteBinarySize(Function->getNonLocalMemoryChanges());
  }

  // write the top-level function list
  Written += writeBinary(*TraceOut, RecordedTopLevelFunctions);

  // write child lists and non-local change lists
  for (auto const &Function: RecordedFunctions) {
    Written += writeBinary(*TraceOut, Function->getChildren());
    Written += writeBinary(*TraceOut, Function->getNonLocalMemoryChanges());
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

void TraceThreadListener::recordMalloc(uint64_t Address, uint64_t Size) {
  ProcessTime = getCIProcessTime();
  
  auto Offset = EventsOut.write<EventType::Malloc>(Size, ProcessTime);

  // update dynamic allocation lookup
  ProcessListener.setCurrentDynamicMemoryAllocation(Address,
                                                    ThreadID,
                                                    Offset,
                                                    Size);
}

DynamicAllocation TraceThreadListener::recordFree(uint64_t Address) {
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

void TraceThreadListener::recordFreeAndClear(uint64_t Address) {
  auto Malloc = recordFree(Address);
  
  // Clear the state of the freed area.
  recordStateClear(Malloc.address(), Malloc.size());
}


//------------------------------------------------------------------------------
// Memory states
//------------------------------------------------------------------------------

void TraceThreadListener::recordUntypedState(char const *Data, uint64_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  uint64_t Address = (uintptr_t) Data;

  ProcessTime = getCIProcessTime();
  

  // update the process' memory trace with the new state, and find the states
  // that were overwritten.
  auto OverwrittenInfo = ProcessListener.addMemoryState(Address,
                                                        Size,
                                                        ThreadID,
                                                        EventsOut.offset(),
                                                        ProcessTime);
  
  auto &OverwrittenStates = OverwrittenInfo.states();
  
  if (Size <= EventRecord<EventType::StateUntypedSmall>::sizeofData()) {
    EventRecord<EventType::StateUntypedSmall>::typeofData DataStore;
    char *DataStorePtr = reinterpret_cast<char *>(&DataStore);
    
    memcpy(DataStorePtr, Data, Size);
    
    EventsOut.write<EventType::StateUntypedSmall>(
      static_cast<uint8_t>(Size),
      static_cast<uint32_t>(OverwrittenStates.size()),
      Address,
      ProcessTime,
      DataStore);
  }
  else {
    auto DataOffset = ProcessListener.recordData(Data, Size);

    // write the state information to the trace
    EventsOut.write<EventType::StateUntyped>(
      static_cast<uint32_t>(OverwrittenStates.size()),
      Address,
      ProcessTime,
      DataOffset,
      Size);
  }
  
  writeStateOverwritten(OverwrittenStates.begin(), OverwrittenStates.end());
  
  // TODO: add non-local change records for all appropriate functions
}

void TraceThreadListener::recordTypedState(void const *Data,
                                           uint64_t Size,
                                           offset_uint Value){
  recordUntypedState(reinterpret_cast<char const *>(Data), Size);
  
#if 0
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  uint64_t Address = (uintptr_t)Data;
  
  ProcessTime = getCIProcessTime();
  
  // update the process' memory trace with the new state, and find the states
  // that were overwritten.
  auto OverwrittenInfo = ProcessListener.addMemoryState(Address,
                                                        Size,
                                                        ThreadID,
                                                        EventsOut.offset(),
                                                        ProcessTime);
  
  auto &OverwrittenStates = OverwrittenInfo.states();
  
  EventsOut.write<EventType::StateTyped>(
    static_cast<uint32_t>(OverwrittenStates.size()),
    Address,
    ProcessTime,
    Value);
  
  writeStateOverwritten(OverwrittenStates.begin(), OverwrittenStates.end());
#endif
}

void TraceThreadListener::recordStateClear(uint64_t Address, uint64_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  
  ProcessTime = getCIProcessTime();
  
  auto OverwrittenInfo = ProcessListener.clearMemoryState(Address, Size);
  
  auto &OverwrittenStates = OverwrittenInfo.states();
  
  EventsOut.write<EventType::StateClear>(
    static_cast<uint32_t>(OverwrittenStates.size()),
    Address,
    ProcessTime,
    Size);
  
  writeStateOverwritten(OverwrittenStates.begin(), OverwrittenStates.end());
}

void TraceThreadListener::recordMemset() {
  llvm_unreachable("recordMemset unimplemented");
}

void TraceThreadListener::recordMemcpy() {
  llvm_unreachable("recordMemcpy unimplemented");
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
