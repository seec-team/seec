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

#define _POSIX_C_SOURCE 199506L

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#endif

#include <type_traits>

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
  // This function polls for blocked signals.
  
#if defined(__APPLE__)
  // Apple don't have sigtimedwait(), so we have to use a messy workaround.
  
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

#elif defined(_POSIX_VERSION) && _POSIX_VERSION >= 199309L
  // Use sigtimedwait() if we possibly can.
  
  sigset_t FullSet;
  auto const FillResult = sigfillset(&FullSet);
  assert(FillResult == 0 && "sigfillset() failed.");
  
  siginfo_t Information;
  struct timespec WaitTime = (struct timespec){.tv_sec = 0, .tv_nsec = 0};
  int const Caught = sigtimedwait(&FullSet, &Information, &WaitTime);
  
  // If no signal is found then sigtimedwait() returns an error (-1).
  if (Caught == -1)
    return;

#else
  // All other platforms - no implementation currently.
  return;

#endif

  // Caught > 0 indicates the signal number of the caught signal.
  switch (Caught) {
    // The default disposition for these signals is to ignore them, so we will
    // also ignore them.
    case SIGCHLD: SEEC_FALLTHROUGH;
    case SIGURG:
      return;
    
    default:
      break;
  }

  // TODO: Write the signal into the trace.
  // Describe signal using strsignal().
  
  // Perform a coordinated exit (traces will be finalized during destruction).
  getSupportSynchronizedExit().getSynchronizedExit().exit(EXIT_FAILURE);
}

std::uintptr_t TraceThreadListener::getRemainingStack() const
{
#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
  struct rlimit lim;
  if (getrlimit(RLIMIT_STACK, &lim) != 0)
    return std::numeric_limits<std::uintptr_t>::max();

  // Determine the size of the existing stack.
  std::uintptr_t StackLow = 0;
  std::uintptr_t StackHigh = 0;

  {
    std::lock_guard<std::mutex> Lock{FunctionStackMutex};
    auto const FrontArea = FunctionStack.front()->getStackArea();
    auto const BackArea = FunctionStack.back()->getStackArea();

    // Pick the lowest non-zero value from the area starts.
    if (FrontArea.start() != 0 && FrontArea.start() < BackArea.start())
      StackLow = FrontArea.start();
    else
      StackLow = BackArea.start();

    StackHigh = std::max(FrontArea.last(), BackArea.last());
  }

  auto const Used = StackHigh - StackLow;
  auto const Remaining = lim.rlim_cur - Used;

  // Reserve 10KiB for SeeC's stack (and general inaccuracy in the measurement).
  constexpr std::uintptr_t SeeCReserved = 10 * 1024;

  return (Remaining > SeeCReserved) ? (Remaining - SeeCReserved) : 0;
#else
  return std::numeric_limits<std::uintptr_t>::max();
#endif
}

void TraceThreadListener::recordStreamOpen(FILE *Stream,
                                           char const *Filename,
                                           char const *Mode)
{
  acquireStreamsLock();

  ProcessTime = getCIProcessTime();

  auto &Streams = ProcessListener.getStreams(StreamsLock);
  
  auto const FilenameOffset =
    ProcessListener.recordData(Filename, std::strlen(Filename) + 1);
  auto const ModeOffset =
    ProcessListener.recordData(Mode, std::strlen(Mode) + 1);
  
  Streams.streamOpened(Stream, FilenameOffset, ModeOffset);

  EventsOut.write<EventType::FileOpen>(ProcessTime,
                                       reinterpret_cast<uintptr_t>(Stream),
                                       FilenameOffset,
                                       ModeOffset);
}

void TraceThreadListener::recordStreamWrite(FILE *Stream,
                                            llvm::ArrayRef<char> Data)
{
  ProcessTime = getCIProcessTime();

  auto const DataOffset = ProcessListener.recordData(Data.data(), Data.size());

  EventsOut.write<EventType::FileWrite>(ProcessTime,
                                        reinterpret_cast<uintptr_t>(Stream),
                                        DataOffset,
                                        Data.size());
}

void TraceThreadListener::recordStreamWriteFromMemory(FILE *Stream,
                                                      MemoryArea Area)
{
  ProcessTime = getCIProcessTime();

  EventsOut.write<EventType::FileWriteFromMemory>
                 (ProcessTime,
                  reinterpret_cast<uintptr_t>(Stream),
                  Area.start(),
                  Area.length());
}

bool TraceThreadListener::recordStreamClose(FILE *Stream)
{
  acquireStreamsLock();

  ProcessTime = getCIProcessTime();

  auto &Streams = ProcessListener.getStreams(StreamsLock);
  
  auto const Info = Streams.streamInfo(Stream);
  if (!Info)
    return false;

  EventsOut.write<EventType::FileClose>(ProcessTime,
                                        reinterpret_cast<uintptr_t>(Stream),
                                        Info->getFilenameOffset(),
                                        Info->getModeOffset());

  Streams.streamClosed(Stream);
  
  return true;
}


//------------------------------------------------------------------------------
// DIR tracking
//------------------------------------------------------------------------------

void TraceThreadListener::recordDirOpen(void const * const TheDIR,
                                        char const *Filename)
{
  acquireDirsLock();

  ProcessTime = getCIProcessTime();

  auto &Dirs = ProcessListener.getDirs(DirsLock);
  
  auto const FilenameOffset =
    ProcessListener.recordData(Filename, std::strlen(Filename) + 1);
  
  Dirs.DIROpened(TheDIR, FilenameOffset);
  
  EventsOut.write<EventType::DirOpen>(ProcessTime,
                                      reinterpret_cast<uintptr_t>(TheDIR),
                                      FilenameOffset);
}

bool TraceThreadListener::recordDirClose(void const * const TheDIR)
{
  acquireDirsLock();

  ProcessTime = getCIProcessTime();

  auto &Dirs = ProcessListener.getDirs(DirsLock);
  
  auto const Info = Dirs.DIRInfo(TheDIR);
  if (!Info)
    return false;
  
  EventsOut.write<EventType::DirClose>(ProcessTime,
                                       reinterpret_cast<uintptr_t>(TheDIR),
                                       Info->getDirnameOffset());
  
  Dirs.DIRClosed(TheDIR);
  
  return true;
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
  
  // Copy in-memory pointer objects.
  ProcessListener.copyInMemoryPointerObjects(Source, Destination, Size);
  
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

bool TraceThreadListener::isKnownMemoryRegionAt(uintptr_t Address) const
{
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  return ProcessListener.getKnownMemory().count(Address);
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
                                         OutputStreamAllocator &StreamAllocator,
                                         offset_uint const WithThreadEventLimit)
: seec::trace::CallDetector<TraceThreadListener>
                           (ProcessListener.getDetectCallsLookup()),
  ProcessListener(ProcessListener),
  SupportSyncExit(ProcessListener.syncExit()),
  ThreadID(ProcessListener.registerThreadListener(this)),
  ThreadEventLimit(WithThreadEventLimit),
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
  StreamsLock(),
  DirsLock()
{
  traceOpen();
  
#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
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
#endif
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
  OutputEnabled = false;
}

void TraceThreadListener::traceOpen()
{
  EventsOut.open(
    StreamAllocator.getThreadStream(ThreadID,
                                    ThreadSegment::Events,
                                    llvm::sys::fs::OpenFlags::F_Append));
  
  OutputEnabled = true;
}


//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

RuntimeValue const *
TraceThreadListener::getCurrentRuntimeValue(llvm::Instruction const *I) const
{
  auto const ActiveFunc = ActiveFunction;
  if (!ActiveFunc)
    return nullptr;

  auto &FIndex = ActiveFunc->getFunctionIndex();

  auto MaybeIndex = FIndex.getIndexOfInstruction(I);
  if (!MaybeIndex.assigned())
    return nullptr;

  return ActiveFunc->getCurrentRuntimeValue(MaybeIndex.get<0>());
}

seec::Maybe<seec::MemoryArea>
TraceThreadListener::getParamByValArea(llvm::Argument const *Arg) const
{
  auto const ActiveFunc = ActiveFunction;
  if (!ActiveFunc)
    return seec::Maybe<seec::MemoryArea>();

  return ActiveFunc->getParamByValArea(Arg);
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
                 seec::Maybe<uint32_t> PreInstructionIndex)
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
      if (traceEnabled()) {
        llvm::errs() << "\nSeeC: Fatal runtime error detected!"
                        " Replay trace for more details.\n";
      }
      else {
        llvm::errs() << "\nSeeC: Fatal runtime error detected!"
                        " Tracing is disabled. This usually indicates that the"
                        " error occurred in a child process.\n";
      }
      
      // Shut down the tracing.
      SupportSyncExit.getSynchronizedExit().exit(EXIT_FAILURE);
      
      break;
  }
}


} // namespace trace (in seec)

} // namespace seec
