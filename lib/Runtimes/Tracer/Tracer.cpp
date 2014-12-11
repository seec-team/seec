//===- lib/Runtimes/Tracer/Tracer.cpp -------------------------------------===//
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

#include "PrintRunError.hpp"
#include "Tracer.hpp"

#include "seec/DSA/MemoryArea.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceStorage.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/SynchronizedExit.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"
#include "seec/wxWidgets/Config.hpp"
#include "seec/wxWidgets/ConfigTracing.hpp"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Threading.h"

#include "unicode/locid.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <dlfcn.h>
#endif

#include "seec/Transforms/RecordExternal/RecordInfo.h" // needs <cstdint>


namespace seec {

namespace trace {

static constexpr char const *getThreadEventLimitEnvVar() {
  return "SEEC_EVENT_LIMIT";
}

static constexpr char const *getArchiveSizeLimitEnvVar() {
  return "SEEC_ARCHIVE_LIMIT";
}


//------------------------------------------------------------------------------
// ThreadEnvironment
//------------------------------------------------------------------------------

ThreadEnvironment::ThreadEnvironment(ProcessEnvironment &PE)
: Process(PE),
  ThreadTracer(PE.getProcessListener(),
               PE.getStreamAllocator(),
               PE.getThreadEventLimit()),
  FunIndex(nullptr),
  Stack()
{}

ThreadEnvironment::~ThreadEnvironment()
{}

void ThreadEnvironment::checkOutputSize()
{
  if (!ThreadTracer.traceEnabled())
    return;

  if (ThreadTracer.traceEventSize() > Process.getThreadEventLimit()) {
    llvm::errs() << "\nSeeC: Thread event limit reached!\n";

    // Shut down the tracing and archive.
    auto const &SupportSyncExit = ThreadTracer.getSupportSynchronizedExit();
    auto StopCanceller = SupportSyncExit.getSynchronizedExit().stopAll();
    if (!StopCanceller.wasStopped())
      return;

    auto &ProcessListener = Process.getProcessListener();

    for (auto const ThreadListenerPtr : ProcessListener.getThreadListeners()) {
      ThreadListenerPtr->traceWrite();
      ThreadListenerPtr->traceFlush();
      ThreadListenerPtr->traceClose();
    }

    ProcessListener.traceWrite();
    ProcessListener.traceFlush();
    ProcessListener.traceClose();

    Process.archive();

    StopCanceller.cancelStop();
  }
}

void ThreadEnvironment::pushFunction(llvm::Function *Fun) {
  Stack.push_back(FunctionEnvironment{Fun});
  FunIndex = Process.getModuleIndex().getFunctionIndex(Fun);
}

llvm::Function *ThreadEnvironment::popFunction() {
  auto Fun = Stack.back();
  Stack.pop_back();
  
  FunIndex = 
  Stack.empty() ? nullptr
  : Process.getModuleIndex().getFunctionIndex(Stack.back().Function);
  
  return Fun.Function;
}

llvm::Instruction *ThreadEnvironment::getInstruction() const {
  return getFunctionIndex().getInstruction(Stack.back().InstructionIndex);
}


//------------------------------------------------------------------------------
// ProcessEnvironment
//------------------------------------------------------------------------------

/// \brief Get the multiplier to use for a given byte multiple.
///
static uint64_t getMultiplierForBytes(char const *ForUnit)
{
  struct LookupTy {
    char const *Unit;
    uint64_t Multiplier;
  };
  
  LookupTy LookupTable[] = {
    {"KiB", 1024},
    {"K"  , 1024},
    {"MiB", 1024*1024},
    {"M"  , 1024*1024},
    {"GiB", 1024*1024*1024},
    {"G"  , 1024*1024*1024}
  };
  
  auto const LookupLength = sizeof(LookupTable) / sizeof(LookupTable[0]);
  
  for (std::size_t i = 0; i < LookupLength; ++i)
    if(std::strcmp(ForUnit, LookupTable[i].Unit) == 0)
      return LookupTable[i].Multiplier;
  
  return 1;
}

/// \brief Get the number of bytes represented by a string.
///
uint64_t getByteSizeFromEnvVar(char const * const EnvVarName,
                               char const * const StringValue)
{
  char *Remainder = nullptr;
  auto const Value = std::strtoull(StringValue, &Remainder, 10);
  if (Remainder == StringValue) {
    fprintf(stderr, "\nSeeC: Error parsing '%s'.\n", EnvVarName);
    std::exit(EXIT_FAILURE);
  }
  
  // Ensure that the value fits in the given type.
  auto const MaxValue = std::numeric_limits<uint64_t>::max();
  if (Value > MaxValue) {
    fprintf(stderr, "\nSeeC: Value of '%s' is too large.\n", EnvVarName);
    fprintf(stderr, "\tMaximum = %s bytes.\n",
            std::to_string(MaxValue).c_str());
    std::exit(EXIT_FAILURE);
  }
  
  // Consume whitespace to find the units.
  while (*Remainder && std::isblank(*Remainder))
    ++Remainder;
  
  auto const Multiplier = getMultiplierForBytes(Remainder);
  if (Multiplier <= 1)
    return Value;
  
  // Ensure that the final value fits in the given type.
  if (MaxValue / Multiplier < Value) {
    fprintf(stderr, "\nSeeC: Value of '%s' is too large.\n", EnvVarName);
    fprintf(stderr, "\tMaximum = %s bytes.\n",
            std::to_string(MaxValue).c_str());
    std::exit(EXIT_FAILURE);
  }
  
  return Value * Multiplier;
}

/// \brief Get the size limit to use for thread event files.
///
/// NOTE: This function uses std::getenv() and thus is not thread-safe.
///
static offset_uint getUserThreadEventLimit()
{
  auto const EnvVarName = getThreadEventLimitEnvVar();
  if (auto const EnvVar = std::getenv(EnvVarName))
    return getByteSizeFromEnvVar(EnvVarName, EnvVar);

  return seec::getThreadEventLimit() * (1024 * 1024);
}

/// \brief Get the size limit for archiving traces.
///
/// NOTE: This function uses std::getenv() and thus is not thread-safe.
///
static uint64_t getUserArchiveSizeLimit()
{
  auto const EnvVarName = getArchiveSizeLimitEnvVar();
  if (auto const EnvVar = std::getenv(EnvVarName))
    return getByteSizeFromEnvVar(EnvVarName, EnvVar);

  return seec::getArchiveLimit() * (1024 * 1024);
}

ProcessEnvironment::ProcessEnvironment()
: Context(),
  Mod(),
  ModIndex(),
  StreamAllocator(),
  SyncExit(),
  ICUResourceLoader(new ResourceLoader(__SeeC_ResourcePath__)),
  Augmentations(new AugmentationCollection()),
  ProcessTracer(),
  ThreadLookup(),
  ThreadLookupMutex(),
  InterceptorAddresses(),
  ThreadEventLimit(0), // set after wxConfig is available.
  ArchiveSizeLimit(0), // set after wxConfig is available.
  ProgramName()
{
  // Setup multithreading support for LLVM.
  llvm::llvm_start_multithreaded();
  
  // Parse the Module bitcode, which is stored in a global variable.
  llvm::StringRef BitcodeRef {
    SeeCInfoModuleBitcode,
    static_cast<std::size_t>(SeeCInfoModuleBitcodeLength)
  };
  
  auto BitcodeBuffer = llvm::MemoryBuffer::getMemBuffer(BitcodeRef, "", false);
  
  Mod.reset(llvm::ParseBitcodeFile(BitcodeBuffer, Context));
  
  if (!Mod) {
    llvm::errs() << "\nSeeC: Failed to parse module bitcode.\n";
    exit(EXIT_FAILURE);
  }
  
  // Create the output stream allocator.
  auto MaybeOutput = OutputStreamAllocator::createOutputStreamAllocator();
  
  if (MaybeOutput.assigned(0)) {
    StreamAllocator = std::move(MaybeOutput.get<0>());
  }
  else {
    llvm::errs() << "\nSeeC: Failed to create output stream allocator.\n";
    if (MaybeOutput.assigned<seec::Error>())
      llvm::errs() << MaybeOutput.get<seec::Error>();
    exit(EXIT_FAILURE);
  }
  
  // Attempt to load ICU resources.
  ICUResourceLoader->loadResource("Trace");
  ICUResourceLoader->loadResource("RuntimeErrors");

  // Setup a dummy wxApp to enable some wxWidgets functionality.
  seec::setupDummyAppConsole();
  seec::setupCommonConfig();

  // Setup limits.
  ThreadEventLimit = getUserThreadEventLimit();
  ArchiveSizeLimit = getUserArchiveSizeLimit();

  // Attempt to load augmentations.
  Augmentations->loadFromResources(__SeeC_ResourcePath__);
  Augmentations->loadFromUserLocalDataDir();

  // Write a copy of the Module's bitcode into the trace directory.
  StreamAllocator->writeModule(BitcodeRef);
  
  // Build ModIndex.
  ModIndex.reset(new ModuleIndex(*Mod));
  
  // Create the process tracer.
  ProcessTracer.reset(new TraceProcessListener(*Mod, 
                                               *ModIndex, 
                                               *StreamAllocator,
                                               SyncExit));

  // Setup runtime error printing.
  ProcessTracer->setRunErrorCallback(
    [this] (seec::runtime_errors::RunError const &Error,
            llvm::Instruction const *Instruction)
    {
      return PrintRunError(Error, Instruction, *ModIndex, *Augmentations);
    });

  // Give the listener the run-time locations of functions.
  uint32_t FunIndex = 0;
  for (auto &Fun: *Mod) {
    if (Fun.isIntrinsic())
      continue;

    ProcessTracer->notifyFunction(FunIndex,
                                  &Fun,
                                  SeeCInfoFunctions[FunIndex]);
    
    ++FunIndex;
  }

  // Give the listener the run-time locations of globals.
  uint32_t GlobalIndex = 0;
  for (auto GlobalIt = Mod->global_begin(), GlobalEnd = Mod->global_end();
       GlobalIt != GlobalEnd; ++GlobalIt) {
    ProcessTracer->notifyGlobalVariable(GlobalIndex,
                                        &*GlobalIt,
                                        SeeCInfoGlobals[GlobalIndex]);
    ++GlobalIndex;
  }

  ProcessTracer->notifyGlobalVariablesComplete();
  
  // Find the location of all intercepted functions.
#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#define SEEC__STR2(NAME) #NAME
#define SEEC__STR(NAME) SEEC__STR2(NAME)

#define SEEC_INTERCEPTED_FUNCTION(NAME)                                        \
  if (auto Ptr = dlsym(RTLD_DEFAULT, SEEC__STR(SEEC_MANGLE_FUNCTION(NAME))))   \
    InterceptorAddresses.insert(reinterpret_cast<uintptr_t>(Ptr));
#include "seec/Runtimes/Tracer/InterceptedFunctions.def"

#undef SEEC__STR2
#undef SEEC__STR
#else
#error "Intercepted function locating not implemented for this platform."
#endif
}

ProcessEnvironment::~ProcessEnvironment()
{
  // Finalize the trace.
  auto const OutputEnabled = ProcessTracer->traceEnabled();
  ThreadLookup.clear();
  ProcessTracer.reset();
  if (OutputEnabled)
    archive();
}

ThreadEnvironment *ProcessEnvironment::getOrCreateCurrentThreadEnvironment()
{
  std::lock_guard<std::mutex> Lock{ThreadLookupMutex};

  auto &ThreadEnvPtr = ThreadLookup[std::this_thread::get_id()];

  if (!ThreadEnvPtr)
    ThreadEnvPtr.reset(new ThreadEnvironment(getProcessEnvironment()));

  return ThreadEnvPtr.get();
}

void ProcessEnvironment::setProgramName(llvm::StringRef Name)
{
  ProgramName = llvm::sys::path::filename(Name);
}

TraceArchiveResult ProcessEnvironment::archive()
{
  // Determine the size of the trace.
  auto const MaybeSize = StreamAllocator->getTotalSize();
  if (MaybeSize.assigned<Error>())
    // TODO: Attempt to get an informative message from the Error.
    return TraceArchiveResult{false, "", "Couldn't read trace file size."};

  if (MaybeSize.get<uint64_t>() > ArchiveSizeLimit)
    return TraceArchiveResult{false, "", "Trace exceeds archive limit."};

  // Attempt to create the archive. If the ProgramName is empty, a name will
  // be created based on the trace directory's name.
  auto MaybeArchived = StreamAllocator->archiveTo(ProgramName);
  if (MaybeArchived.assigned<std::string>())
    return TraceArchiveResult{true, MaybeArchived.move<std::string>(), ""};
  else if (MaybeArchived.assigned<Error>())
    // TODO: Attempt to get an informative message from the Error.
    return TraceArchiveResult{false, "", "Failed to archive the trace."};
  else {
    llvm_unreachable("No result from archiveTo()!");
    return TraceArchiveResult{false, "", "Failed to archive the trace."};
  }
}

TraceArchiveResult
ProcessEnvironment::unarchive(TraceArchiveResult const &FromArchive)
{
  if (!FromArchive.getSuccess())
    return TraceArchiveResult{false, FromArchive.getFilename(), ""};

  auto const MaybeErr = StreamAllocator->extractFrom(FromArchive.getFilename());
  if (MaybeErr.assigned<Error>())
    // TODO: Attempt to get an informative message from the Error.
    return TraceArchiveResult{false, FromArchive.getFilename(),
                              "Couldn't extract trace file."};

  return TraceArchiveResult{true, FromArchive.getFilename(), ""};
}


//------------------------------------------------------------------------------
// getProcessEnvironment()
//------------------------------------------------------------------------------

ProcessEnvironment &getProcessEnvironment() {
  static std::once_flag InitFlag;
  static std::unique_ptr<ProcessEnvironment> ProcessEnv;

  std::call_once(InitFlag, [&](){
    ProcessEnv.reset(new ProcessEnvironment());
  });

  return *ProcessEnv;
}


//------------------------------------------------------------------------------
// getThreadEnvironment()
//------------------------------------------------------------------------------

ThreadEnvironment &getThreadEnvironment() {
#if __has_feature(cxx_thread_local)
  // Keep a thread-local pointer to this thread's environment.
  thread_local ThreadEnvironment *TE =
    getProcessEnvironment().getOrCreateCurrentThreadEnvironment();
#else
  // Keep a thread-local pointer to this thread's environment.
  static __thread ThreadEnvironment *TE =
    getProcessEnvironment().getOrCreateCurrentThreadEnvironment();
#endif

  assert(TE && "ThreadEnvironment not found!");

  return *TE;
}


} // namespace trace (in seec)

} // namespace seec


extern "C" {

void SeeCRecordFunctionBegin(uint32_t Index) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &ModIndex = seec::trace::getProcessEnvironment().getModuleIndex();
  auto &Listener = ThreadEnv.getThreadListener();
  auto F = ModIndex.getFunction(Index);
  Listener.notifyFunctionBegin(Index, F);
  ThreadEnv.pushFunction(F);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordFunctionEnd(uint32_t Index, uint32_t const InstructionIndex) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();

  auto const &FIndex = ThreadEnv.getFunctionIndex();
  auto F = ThreadEnv.popFunction();
  auto I = FIndex.getInstruction(InstructionIndex);

  // Check F matches Index?
  Listener.notifyFunctionEnd(Index, F, InstructionIndex, I);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordArgumentByVal(uint32_t Index, void *Address) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  
  auto Arg = ThreadEnv.getFunctionIndex().getArgument(Index);
  assert(Arg && "Expected Argument");
  
  Listener.notifyArgumentByVal(Index, Arg, Address);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordArgs(int64_t ArgC, char **ArgV) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyArgs(ArgC, ArgV);
  
  if (ArgC) {
    auto &ProcessEnv = ThreadEnv.getProcessEnvironment();
    ProcessEnv.setProgramName(ArgV[0]);
  }

  ThreadEnv.checkOutputSize();
}

void SeeCRecordEnv(char **EnvP) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyEnv(EnvP);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordSetInstruction(uint32_t Index) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);
}

void SeeCRecordPreAlloca(uint32_t const Index,
                         uint64_t const ElemSize,
                         uint64_t const ElemCount)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);
  auto const Alloca = llvm::cast<llvm::AllocaInst>(ThreadEnv.getInstruction());
  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreAlloca(Index, *Alloca, ElemSize, ElemCount);
}

void SeeCRecordPreLoad(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);

  auto Load = llvm::dyn_cast<llvm::LoadInst>(ThreadEnv.getInstruction());
  assert(Load && "Expected LoadInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreLoad(Index, Load, Address, Size);
}

void SeeCRecordPostLoad(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();

  auto Load = llvm::dyn_cast<llvm::LoadInst>(ThreadEnv.getInstruction());
  assert(Load && "Expected LoadInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPostLoad(Index, Load, Address, Size);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordPreStore(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);
  
  auto Store = llvm::dyn_cast<llvm::StoreInst>(ThreadEnv.getInstruction());
  assert(Store && "Expected StoreInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreStore(Index, Store, Address, Size);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordPostStore(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  
  auto Store = llvm::dyn_cast<llvm::StoreInst>(ThreadEnv.getInstruction());
  assert(Store && "Expected StoreInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPostStore(Index, Store, Address, Size);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordPreCall(uint32_t Index, void *Address) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);
  
  auto &ProcessEnv = seec::trace::getProcessEnvironment();
  if (ProcessEnv.isInterceptedFunction(reinterpret_cast<uintptr_t>(Address))) {
    ThreadEnv.setInstructionIsInterceptedCall();
  }
  else {
    auto Call = llvm::dyn_cast<llvm::CallInst>(ThreadEnv.getInstruction());
    assert(Call && "Expected CallInst");

    auto &Listener = ThreadEnv.getThreadListener();
    Listener.notifyPreCall(Index, Call, Address);
  }
}

void SeeCRecordPostCall(uint32_t Index, void *Address) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  if (!ThreadEnv.getInstructionIsInterceptedCall()) {
    auto Call = llvm::dyn_cast<llvm::CallInst>(ThreadEnv.getInstruction());
    assert(Call && "Expected CallInst");

    auto &Listener = ThreadEnv.getThreadListener();
    Listener.notifyPostCall(Index, Call, Address);
  }

  ThreadEnv.checkOutputSize();
}

void SeeCRecordPreCallIntrinsic(uint32_t Index) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);

  auto Call = llvm::dyn_cast<llvm::CallInst>(ThreadEnv.getInstruction());
  assert(Call && "Expected CallInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreCallIntrinsic(Index, Call);
}

void SeeCRecordPostCallIntrinsic(uint32_t Index) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();

  auto Call = llvm::dyn_cast<llvm::CallInst>(ThreadEnv.getInstruction());
  assert(Call && "Expected CallInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPostCallIntrinsic(Index, Call);

  ThreadEnv.checkOutputSize();
}

void SeeCRecordPreDivide(uint32_t Index) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);

  auto BinOp = llvm::dyn_cast<llvm::BinaryOperator>(ThreadEnv.getInstruction());
  assert(BinOp && "Expected BinaryOperator");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreDivide(Index, BinOp);
}

void SeeCRecordUpdateVoid(uint32_t Index) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);

  if (ThreadEnv.getInstructionIsInterceptedCall())
    return;

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyValue(Index, ThreadEnv.getInstruction());

  ThreadEnv.checkOutputSize();
}

#define SEEC_RECORD_TYPED(NAME, TYPE)                                          \
void SeeCRecordUpdate##NAME(uint32_t Index, TYPE Value) {                      \
  auto &ThreadEnv = seec::trace::getThreadEnvironment();                       \
  ThreadEnv.setInstructionIndex(Index);                                        \
  if (ThreadEnv.getInstructionIsInterceptedCall())                             \
    return;                                                                    \
  auto &Listener = ThreadEnv.getThreadListener();                              \
  Listener.notifyValue(Index, ThreadEnv.getInstruction(), Value);              \
  ThreadEnv.checkOutputSize();                                                 \
}                                                                              \
void SeeCRecordSetCurrent##NAME(TYPE Value) {                                  \
  auto &ThreadEnv = seec::trace::getThreadEnvironment();                       \
  auto &Listener = ThreadEnv.getThreadListener();                              \
  Listener.notifyValue(ThreadEnv.getInstructionIndex(),                        \
                       ThreadEnv.getInstruction(),                             \
                       Value);                                                 \
  ThreadEnv.checkOutputSize();                                                 \
}

SEEC_RECORD_TYPED(Pointer, void *)
SEEC_RECORD_TYPED(Int64,   uint64_t)
SEEC_RECORD_TYPED(Int32,   uint32_t)
SEEC_RECORD_TYPED(Int16,   uint16_t)
SEEC_RECORD_TYPED(Int8,    uint8_t)
SEEC_RECORD_TYPED(Float,   float)
SEEC_RECORD_TYPED(Double,  double)
SEEC_RECORD_TYPED(X86FP80, long double)

// UpdateFP128
// UpdatePPC128

#undef SEEC_RECORD_TYPED

void SEEC_MANGLE_FUNCTION(LockDynamicMemory)() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.acquireDynamicMemoryLock();
}

void
SEEC_MANGLE_FUNCTION(RecordMalloc)
(void const * const Address, size_t const Size)
{
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.recordMalloc(reinterpret_cast<uintptr_t>(Address), Size);
}

void SEEC_MANGLE_FUNCTION(RecordFree)(void const * const Address) {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.recordFreeAndClear(reinterpret_cast<uintptr_t>(Address));
}

void SEEC_MANGLE_FUNCTION(LockMemoryForWriting)() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.acquireGlobalMemoryWriteLock();
}

void SEEC_MANGLE_FUNCTION(LockMemoryForReading)() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.acquireGlobalMemoryReadLock();
}

char SEEC_MANGLE_FUNCTION(IsKnownMemoryCovering)(void const * const Start,
                                                 size_t const Size)
{
  auto const Address = reinterpret_cast<uintptr_t>(Start);

  return seec::trace::getThreadEnvironment()
                     .getThreadListener()
                     .isKnownMemoryRegionCovering(Address, Size);
}

void SEEC_MANGLE_FUNCTION(RemoveKnownMemory)(void const * const Start) {
  auto const Address = reinterpret_cast<uintptr_t>(Start);

  seec::trace::getThreadEnvironment()
              .getThreadListener()
              .removeKnownMemoryRegion(Address);
}

void SEEC_MANGLE_FUNCTION(AddKnownMemory)(void const * const Start,
                                          size_t const Size,
                                          char const Readable,
                                          char const Writable)
{
  auto const Address = reinterpret_cast<uintptr_t>(Start);
  auto const Permission
    = Readable ? (Writable ? seec::MemoryPermission::ReadWrite
                           : seec::MemoryPermission::ReadOnly)
               : (Writable ? seec::MemoryPermission::WriteOnly
                           : seec::MemoryPermission::None);

  seec::trace::getThreadEnvironment()
              .getThreadListener()
              .addKnownMemoryRegion(Address, Size, Permission);
}

void
SEEC_MANGLE_FUNCTION(RecordUntypedState)
(char const * const Data, size_t const Size)
{
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.recordUntypedState(Data, Size);
}

void SEEC_MANGLE_FUNCTION(ReleaseLocks)() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.exitPostNotification();
}

void SEEC_MANGLE_FUNCTION(SetPointerTargetNewValid)(void const * const Pointer)
{
  auto &Thread = seec::trace::getThreadEnvironment().getThreadListener();
  auto const Address = reinterpret_cast<uintptr_t>(Pointer);
  auto const ActiveFn = Thread.getActiveFunction();

  assert(ActiveFn);

  auto const Area = seec::trace::getContainingMemoryArea(Thread, Address);
  auto const Target = Area.assigned() ? Area.get<seec::MemoryArea>().start()
                                      : Address;

  ActiveFn->setPointerObject(ActiveFn->getActiveInstruction(),
                             Thread.getProcessListener()
                                   .makePointerObject(Target));
}

void SEEC_MANGLE_FUNCTION(SetPointerTargetFromArgument)(unsigned const ArgNo)
{
  auto const ActiveFn = seec::trace::getThreadEnvironment()
                                    .getThreadListener()
                                    .getActiveFunction();

  assert(ActiveFn);

  ActiveFn->transferArgPointerObjectToCall(ArgNo);
}

void SEEC_MANGLE_FUNCTION(CheckCStringRead)(size_t const Parameter,
                                            char const * const CString)
{
  using namespace seec::runtime_errors::format_selects;
  
  auto &Env = seec::trace::getThreadEnvironment();
  
  seec::trace::CStdLibChecker Checker {Env.getThreadListener(),
                                       Env.getInstructionIndex(),
                                       CStdFunction::userdefined};
  
  Checker.checkCStringRead(Parameter, CString);
}

} // extern "C"
