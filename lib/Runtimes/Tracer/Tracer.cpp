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

#include "seec/ICU/Resources.hpp"
#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceStorage.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/SynchronizedExit.hpp"

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

static constexpr offset_uint getDefaultThreadEventLimit() {
  return 1024 * 1024 * 1024; // 1 GiB
}

static constexpr char const *getArchiveSizeLimitEnvVar() {
  return "SEEC_ARCHIVE_LIMIT";
}

static constexpr uint64_t getDefaultArchiveSizeLimit() {
  return 512 * 1024 * 1024; // 0.5 GiB
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
template<typename T>
T getByteSizeFromEnvVar(char const * const EnvVarName, T const Default)
{
  auto const StringValue = std::getenv(EnvVarName);
  if (!StringValue)
    return Default;
  
  char *Remainder = nullptr;
  auto const Value = std::strtoull(StringValue, &Remainder, 10);
  if (Remainder == StringValue) {
    fprintf(stderr, "\nSeeC: Error parsing '%s'.\n", EnvVarName);
    std::exit(EXIT_FAILURE);
  }
  
  // Ensure that the value fits in the given type.
  auto const MaxValue = std::numeric_limits<T>::max();
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
  return getByteSizeFromEnvVar(getThreadEventLimitEnvVar(),
                               getDefaultThreadEventLimit());
}

/// \brief Get the size limit for archiving traces.
///
/// NOTE: This function uses std::getenv() and thus is not thread-safe.
///
static uint64_t getUserArchiveSizeLimit()
{
  return getByteSizeFromEnvVar(getArchiveSizeLimitEnvVar(),
                               getDefaultArchiveSizeLimit());
}

void ProcessEnvironment::archive()
{
  // Determine the size of the trace.
  auto const MaybeSize = StreamAllocator->getTotalSize();
  if (MaybeSize.assigned<seec::Error>()) {
    fprintf(stderr, "\nSeeC: Couldn't read trace file size.\n");
    return;
  }
  
  if (MaybeSize.get<uint64_t>() > ArchiveSizeLimit) {
    fprintf(stderr, "\nSeeC: Deciding to not archive the trace due to size.\n");
    return;
  }
  
  // Attempt to create the archive. If the ProgramName is empty, a name will
  // be created based on the trace directory's name.
  auto const MaybeError = StreamAllocator->archiveTo(ProgramName);
  if (MaybeError.assigned<seec::Error>()) {
    fprintf(stderr, "\nSeeC: Failed to archive the trace.\n");
    return;
  }
}

ProcessEnvironment::ProcessEnvironment()
: Context(),
  Mod(),
  ModIndex(),
  StreamAllocator(),
  SyncExit(),
  ICUResourceLoader(new ResourceLoader(__SeeC_ResourcePath__)),
  ProcessTracer(),
  ThreadLookup(),
  ThreadLookupMutex(),
  InterceptorAddresses(),
  ThreadEventLimit(getUserThreadEventLimit()),
  ArchiveSizeLimit(getUserArchiveSizeLimit()),
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
      return PrintRunError(Error, Instruction, *ModIndex);
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
}

void SeeCRecordFunctionEnd(uint32_t Index, uint32_t const InstructionIndex) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();

  auto const &FIndex = ThreadEnv.getFunctionIndex();
  auto F = ThreadEnv.popFunction();
  auto I = FIndex.getInstruction(InstructionIndex);

  // Check F matches Index?
  Listener.notifyFunctionEnd(Index, F, InstructionIndex, I);
}

void SeeCRecordArgumentByVal(uint32_t Index, void *Address) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  
  auto Arg = ThreadEnv.getFunctionIndex().getArgument(Index);
  assert(Arg && "Expected Argument");
  
  Listener.notifyArgumentByVal(Index, Arg, Address);
}

void SeeCRecordSetReadable(void *Address, uint64_t Size) {
  llvm::errs() << "readable " << Address << ", " << Size << "\n";
}

void SeeCRecordSetWritable(void *Address, uint64_t Size) {
  llvm::errs() << "readable " << Address << ", " << Size << "\n";
}

void SeeCRecordArgs(int64_t ArgC, char **ArgV) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyArgs(ArgC, ArgV);
  
  if (ArgC) {
    auto &ProcessEnv = ThreadEnv.getProcessEnvironment();
    ProcessEnv.setProgramName(ArgV[0]);
  }
}

void SeeCRecordEnv(char **EnvP) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyEnv(EnvP);
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
}

void SeeCRecordPreStore(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);
  
  auto Store = llvm::dyn_cast<llvm::StoreInst>(ThreadEnv.getInstruction());
  assert(Store && "Expected StoreInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreStore(Index, Store, Address, Size);
}

void SeeCRecordPostStore(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  
  auto Store = llvm::dyn_cast<llvm::StoreInst>(ThreadEnv.getInstruction());
  assert(Store && "Expected StoreInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPostStore(Index, Store, Address, Size);
}

void SeeCRecordPreCall(uint32_t Index, void *Address) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  ThreadEnv.setInstructionIndex(Index);
  
  auto &ProcessEnv = seec::trace::getProcessEnvironment();
  if (ProcessEnv.isInterceptedFunction(reinterpret_cast<uintptr_t>(Address))) {
    ThreadEnv.setInstructionIsInterceptedCall();
    return;
  }
  
  auto Call = llvm::dyn_cast<llvm::CallInst>(ThreadEnv.getInstruction());
  assert(Call && "Expected CallInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPreCall(Index, Call, Address);
}

void SeeCRecordPostCall(uint32_t Index, void *Address) {
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  if (ThreadEnv.getInstructionIsInterceptedCall())
    return;
  
  auto Call = llvm::dyn_cast<llvm::CallInst>(ThreadEnv.getInstruction());
  assert(Call && "Expected CallInst");

  auto &Listener = ThreadEnv.getThreadListener();
  Listener.notifyPostCall(Index, Call, Address);
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
}

#define SEEC_RECORD_TYPED(NAME, TYPE)                                          \
void SeeCRecordUpdate##NAME(uint32_t Index, TYPE Value) {                      \
  auto &ThreadEnv = seec::trace::getThreadEnvironment();                       \
  ThreadEnv.setInstructionIndex(Index);                                        \
  if (ThreadEnv.getInstructionIsInterceptedCall())                             \
    return;                                                                    \
  auto &Listener = ThreadEnv.getThreadListener();                              \
  Listener.notifyValue(Index, ThreadEnv.getInstruction(), Value);              \
}                                                                              \
void SeeCRecordSetCurrent##NAME(TYPE Value) {                                  \
  auto &ThreadEnv = seec::trace::getThreadEnvironment();                       \
  auto &Listener = ThreadEnv.getThreadListener();                              \
  Listener.notifyValue(ThreadEnv.getInstructionIndex(),                        \
                       ThreadEnv.getInstruction(),                             \
                       Value);                                                 \
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

void SEEC_MANGLE_FUNCTION(CheckCStringRead)(char const * const CString) {
  using namespace seec::runtime_errors::format_selects;
  
  auto &Env = seec::trace::getThreadEnvironment();
  
  seec::trace::CStdLibChecker Checker {Env.getThreadListener(),
                                       Env.getInstructionIndex(),
                                       CStdFunction::userdefined};
  
  Checker.checkCStringRead(1, CString);
}

} // extern "C"
