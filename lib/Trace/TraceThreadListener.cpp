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

// We need this under xcode9, otherwise the presence of _POSIX_SOURCE causes
// some functions to not be defined. Those functions are later referenced by
// __threading_support in libc++, breaking the compile.
#if (defined(__APPLE__))
#define _DARWIN_C_SOURCE
#endif

#include "seec/ICU/Output.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceRecordedFunction.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/Fallthrough.hpp"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <unistd.h>
#include <sys/resource.h>
#endif

#include <algorithm>
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
    
    EventsOut->write<EventType::NewProcessTime>(ProcessTime);
  }
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
    auto const FrontArea = FunctionStack.front().getStackArea();
    auto const BackArea = FunctionStack.back().getStackArea();

    // Pick the lowest non-zero value from the area starts.
    if (FrontArea.start() != 0 && FrontArea.start() < BackArea.start())
      StackLow = FrontArea.start();
    else
      StackLow = BackArea.start();

    StackHigh = std::max(FrontArea.last(), BackArea.last());
  }

  auto const Used = StackHigh - StackLow;
  auto const Remaining = lim.rlim_cur - Used;

  // Reserve 100KiB for SeeC's stack (and general inaccuracy in measurement).
  constexpr std::uintptr_t SeeCReserved = 100 * 1024;

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

  if (FilenameOffset && ModeOffset) {
    EventsOut->write<EventType::FileOpen>(ProcessTime,
                                          reinterpret_cast<uintptr_t>(Stream),
                                          *FilenameOffset,
                                          *ModeOffset);
  }
}

void TraceThreadListener::recordStreamWrite(FILE *Stream,
                                            llvm::ArrayRef<char> Data)
{
  ProcessTime = getCIProcessTime();

  auto const DataOffset = ProcessListener.recordData(Data.data(), Data.size());
  
  if (DataOffset) {
    EventsOut->write<EventType::FileWrite>(ProcessTime,
                                           reinterpret_cast<uintptr_t>(Stream),
                                           *DataOffset,
                                           Data.size());
  }
}

void TraceThreadListener::recordStreamWriteFromMemory(FILE *Stream,
                                                      MemoryArea Area)
{
  ProcessTime = getCIProcessTime();

  EventsOut->write<EventType::FileWriteFromMemory>
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

  auto const FilenameOffset = Info->getFilenameOffset();
  auto const ModeOffset = Info->getModeOffset();
  assert(FilenameOffset && ModeOffset);
  
  EventsOut->write<EventType::FileClose>(ProcessTime,
                                         reinterpret_cast<uintptr_t>(Stream),
                                         *FilenameOffset,
                                         *ModeOffset);

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
  
  if (FilenameOffset) {
    EventsOut->write<EventType::DirOpen>(ProcessTime,
                                         reinterpret_cast<uintptr_t>(TheDIR),
                                         *FilenameOffset);
  }
}

bool TraceThreadListener::recordDirClose(void const * const TheDIR)
{
  acquireDirsLock();

  ProcessTime = getCIProcessTime();

  auto &Dirs = ProcessListener.getDirs(DirsLock);
  
  auto const Info = Dirs.DIRInfo(TheDIR);
  if (!Info)
    return false;
  
  auto const DirnameOffset = Info->getDirnameOffset();
  assert(DirnameOffset);
  
  EventsOut->write<EventType::DirClose>(ProcessTime,
                                        reinterpret_cast<uintptr_t>(TheDIR),
                                        *DirnameOffset);
  
  Dirs.DIRClosed(TheDIR);
  
  return true;
}


//------------------------------------------------------------------------------
// Dynamic memory
//------------------------------------------------------------------------------

void TraceThreadListener::recordMalloc(uintptr_t Address, std::size_t Size) {
  ProcessTime = getCIProcessTime();
  
  auto Write = EventsOut->write<EventType::Malloc>(Size, ProcessTime);
  auto const Offset = Write ? Write->Offset : 0;

  // update dynamic allocation lookup
  ProcessListener.setCurrentDynamicMemoryAllocation(Address,
                                                    ThreadID,
                                                    Offset,
                                                    Size);
}

void TraceThreadListener::recordRealloc(uintptr_t const Address,
                                        std::size_t const NewSize)
{
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");

  auto const Alloc = ProcessListener.getCurrentDynamicMemoryAllocation(Address);
  assert(Alloc && "recordRealloc with unallocated address.");

  auto const OldSize = Alloc->size();

  ProcessTime = getCIProcessTime();
  EventsOut->write<EventType::Realloc>(Address, OldSize, NewSize, ProcessTime);

  {
    auto MemoryState = ProcessListener.getTraceMemoryStateAccessor();
    if (NewSize < OldSize) {
      MemoryState->clear(Address + NewSize,  // Start of cleared memory.
                         OldSize - NewSize); // Length of cleared memory.
    }
    MemoryState->resizeAllocation(Address, NewSize);
  }
  
  ProcessListener.setCurrentDynamicMemoryAllocation(Alloc->address(),
                                                    Alloc->thread(),
                                                    Alloc->offset(),
                                                    NewSize);
  ProcessListener.incrementRegionTemporalID(Address);
}

DynamicAllocation TraceThreadListener::recordFree(uintptr_t Address) {
  auto const Alloc = ProcessListener.getCurrentDynamicMemoryAllocation(Address);

  // If the allocation didn't exist it should have been caught in preCfree.
  assert(Alloc && "recordFree with unassigned address.");

  auto const Malloc = *Alloc; // Copy the DynamicAllocation.

  // Get new process time and update this thread's view of process time.
  ProcessTime = getCIProcessTime();

  // Write Free event.
  EventsOut->write<EventType::Free>(Address, ProcessTime);

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
  
  if (Size == 0)
    return;

  uintptr_t Address = reinterpret_cast<uintptr_t>(Data);

  ProcessTime = getCIProcessTime();
  
  // Update the process' memory trace with the new state.
  auto MemoryState = ProcessListener.getTraceMemoryStateAccessor();
  MemoryState->add(Address, Size);
  
  if (Size <= EventRecord<EventType::StateUntypedSmall>::sizeofData()) {
    EventRecord<EventType::StateUntypedSmall>::typeofData DataStore;
    char *DataStorePtr = reinterpret_cast<char *>(&DataStore);
    memcpy(DataStorePtr, Data, Size);
    
    // Write the state information to the trace.
    EventsOut->write<EventType::StateUntypedSmall>(static_cast<uint8_t>(Size),
                                                   Address,
                                                   ProcessTime,
                                                   DataStore);
  }
  else {
    auto DataOffset = ProcessListener.recordData(Data, Size);

    // Write the state information to the trace.
    if (DataOffset) {
      EventsOut->write<EventType::StateUntyped>(Address,
                                                ProcessTime,
                                                *DataOffset,
                                                Size);
    }
  }
}

void TraceThreadListener::recordTypedState(void const *Data,
                                           std::size_t Size,
                                           offset_uint Value){
  recordUntypedState(reinterpret_cast<char const *>(Data), Size);
}

void TraceThreadListener::recordStateClear(uintptr_t Address,
                                           std::size_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");

  if (Size == 0)
    return;
  
  ProcessTime = getCIProcessTime();
  
  EventsOut->write<EventType::StateClear>(Address,
                                          ProcessTime,
                                          Size);
}

void TraceThreadListener::recordMemset() {
  llvm_unreachable("recordMemset unimplemented");
}

void TraceThreadListener::recordMemmove(uintptr_t Source,
                                        uintptr_t Destination,
                                        std::size_t Size) {
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");

  if (Size == 0)
    return;
  
  ProcessTime = getCIProcessTime();
  
  // Copy in-memory pointer objects.
  ProcessListener.copyInMemoryPointerObjects(Source, Destination, Size);
  
  auto const MemoryState = ProcessListener.getTraceMemoryStateAccessor();
  MemoryState->memmove(Source, Destination, Size);
  
  EventsOut->write<EventType::StateMemmove>(ProcessTime,
                                            Source,
                                            Destination,
                                            Size);
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
  
  EventsOut->write<EventType::KnownRegionAdd>
                  (Address, Length, Readable, Writable);
}

bool TraceThreadListener::isKnownMemoryRegionAt(uintptr_t Address) const
{
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");
  return ProcessListener.getKnownMemory().count(Address);
}

bool TraceThreadListener::isKnownMemoryRegionCovering(uintptr_t const Address,
                                                      std::size_t const Length)
const
{
  assert(GlobalMemoryLock.owns_lock() && "Global memory is not locked.");

  auto &KnownMemory = ProcessListener.getKnownMemory();
  auto const It = KnownMemory.find(Address);

  if (It == KnownMemory.end())
    return false;

  return (It->Begin <= Address && It->End >= Address + Length);
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
  
  EventsOut->write<EventType::KnownRegionRemove>
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
  ThreadID(ProcessListener.registerThreadListener(this)),
  StreamAllocator(StreamAllocator),
  OutputEnabled(false),
  EventsOut(llvm::make_unique<EventWriter>()),
  Time(0),
  ProcessTime(0),
  RecordedFunctions(),
  FunctionStack(),
  ActiveFunction(nullptr),
  GlobalMemoryLock(),
  DynamicMemoryLock(),
  StreamsLock(),
  DirsLock()
{
  EventsOut->open(StreamAllocator.getThreadEventStream(ThreadID));
  OutputEnabled = true;
}

TraceThreadListener::~TraceThreadListener()
{
  traceClose();
  
  ProcessListener.deregisterThreadListener(ThreadID);
}


//------------------------------------------------------------------------------
// Trace writing control.
//------------------------------------------------------------------------------

void TraceThreadListener::traceClose()
{
  EventsOut->close();
  OutputEnabled = false;
}


//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

RuntimeValue const *
TraceThreadListener::getCurrentRuntimeValue(llvm::Instruction const *I) const
{
  if (!ActiveFunction)
    return nullptr;

  auto &FIndex = ActiveFunction->getFunctionIndex();

  auto MaybeIndex = FIndex.getIndexOfInstruction(I);
  if (!MaybeIndex)
    return nullptr;

  return ActiveFunction->getCurrentRuntimeValue(*MaybeIndex);
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

uint64_t TraceThreadListener::incrementThreadTime() {
  EventsOut->write<seec::trace::EventType::NewThreadTime>(++Time);
  CIProcessTime.reset();
  return Time;
}

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
                 llvm::Optional<InstrIndexInFn> PreInstructionIndex)
{
  // PreInstruction event precedes the RuntimeError
  if (PreInstructionIndex) {
    ++Time;
    EventsOut->write<EventType::PreInstruction>(*PreInstructionIndex);
  }
  
  writeError(*EventsOut, Error, true);

  // Call the runtime error callback, if there is one.
  auto const &Callback = ProcessListener.getRunErrorCallback();
  if (Callback) {
    llvm::Instruction const *TheInstruction = nullptr;

    if (!ActiveFunction->isShim()) {
      TheInstruction = ActiveFunction->getActiveInstruction();
      if (PreInstructionIndex) {
        auto const Idx = *PreInstructionIndex;
        TheInstruction = ActiveFunction->getFunctionIndex().getInstruction(Idx);
      }
    }
    else {
      auto const FnIt = std::find_if(FunctionStack.rbegin(),
                                     FunctionStack.rend(),
                                     [] (TracedFunction const &TF) {
                                       return !TF.isShim();
                                     });
      assert(FnIt != FunctionStack.rend() && "Shim with no non-shim parent?");

      TheInstruction = FnIt->getActiveInstruction();
      if (PreInstructionIndex) {
        auto const Idx = *PreInstructionIndex;
        TheInstruction = FnIt->getFunctionIndex().getInstruction(Idx);
      }
    }

    Callback(Error, TheInstruction);
  }
  
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
      
      std::exit(EXIT_FAILURE);
      
      break;
  }
}

void
TraceThreadListener::handleRunError(seec::runtime_errors::RunError const &Error,
                                    RunErrorSeverity Severity)
{
  handleRunError(Error, Severity, llvm::Optional<InstrIndexInFn>());
}


//------------------------------------------------------------------------------
// Mutators
//------------------------------------------------------------------------------

void TraceThreadListener::pushShimFunction()
{
  // A shim cannot be a top-level function.
  assert(!FunctionStack.empty());

  auto &ParentRecord = FunctionStack.back().getRecordedFunction();
  FunctionStack.emplace_back(*this, ParentRecord);
  ActiveFunction = &FunctionStack.back();
}

void TraceThreadListener::popShimFunction()
{
  // TODO: assert that the back of the stack is a shim.
  assert(!FunctionStack.empty());
  FunctionStack.pop_back();
  ActiveFunction = FunctionStack.empty() ? nullptr : &FunctionStack.back();
}


} // namespace trace (in seec)

} // namespace seec
