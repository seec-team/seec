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

#include "Tracer.hpp"

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
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Threading.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <dlfcn.h>

#include "seec/Transforms/RecordExternal/RecordInfo.h" // needs <cstdint>


namespace seec {

namespace trace {

static constexpr char const *getThreadEventLimitEnvVar() {
  return "SEEC_EVENT_LIMIT";
}

static constexpr offset_uint getDefaultThreadEventLimit() {
  return 1024 * 1024 * 1024; // 1 GiB
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

/// \brief Get the size limit to use for thread event files.
///
/// NOTE: This function uses std::getenv() and thus is not thread-safe.
///
static offset_uint getUserThreadEventLimit()
{
  auto const EnvVarName = getThreadEventLimitEnvVar();
  auto const UserLimit = std::getenv(EnvVarName);
  if (!UserLimit)
    return getDefaultThreadEventLimit();
  
  char *Remainder = nullptr;
  auto const Value = std::strtoull(UserLimit, &Remainder, 10);
  
  if (Remainder == UserLimit) {
    llvm::errs() << "SeeC: Error reading " << EnvVarName << ".\n";
    std::exit(EXIT_FAILURE);
  }
  
  if (Value > std::numeric_limits<offset_uint>::max()) {
    llvm::errs() << "SeeC: " << EnvVarName << " is too large.\n";
    llvm::errs() << "\tMaximum = "
                << std::numeric_limits<offset_uint>::max()
                << " bytes.\n";
    std::exit(EXIT_FAILURE);
  }
  
  // Consume whitespace.
  while (*Remainder && std::isblank(*Remainder))
    ++Remainder;
  
  auto const Multiplier = getMultiplierForBytes(Remainder);
  if (Multiplier <= 1)
    return Value;
  
  if (std::numeric_limits<offset_uint>::max() / Multiplier < Value) {
    llvm::errs() << "SeeC: " << EnvVarName << " is too large.\n";
    llvm::errs() << "\tMaximum = "
                << std::numeric_limits<offset_uint>::max()
                << " bytes.\n";
    std::exit(EXIT_FAILURE);
  }
  
  return Value * Multiplier;
}

ProcessEnvironment::ProcessEnvironment()
: Context(),
  Mod(),
  ModIndex(),
  StreamAllocator(),
  SyncExit(),
  ProcessTracer(),
  ThreadLookup(),
  InterceptorAddresses(),
  ThreadEventLimit(getUserThreadEventLimit())
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
    llvm::errs() << "\nFailed to parse module bitcode.\n";
    exit(EXIT_FAILURE);
  }
  
  // Create the output stream allocator.
  auto MaybeOutput = OutputStreamAllocator::
                     createOutputStreamAllocator(Mod->getModuleIdentifier());
  
  if (MaybeOutput.assigned(0)) {
    StreamAllocator = std::move(MaybeOutput.get<0>());
  }
  else if (MaybeOutput.assigned(1)) {
    llvm::errs() << "\nError returned from createOutputStreamAllocator().\n";
    exit(EXIT_FAILURE);
  }
  else {
    llvm::errs() << "\nNo return from createOutputStreamAllocator().\n";
    exit(EXIT_FAILURE);
  }
  
  // Write a copy of the Module's bitcode into the trace directory.
  StreamAllocator->writeModule(BitcodeRef);
  
  // Build ModIndex.
  ModIndex.reset(new ModuleIndex(*Mod));
  
  // Create the process tracer.
  ProcessTracer.reset(new TraceProcessListener(*Mod, 
                                               *ModIndex, 
                                               *StreamAllocator,
                                               SyncExit));

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
  
  // Find the location of all intercepted functions.
#define SEEC__STR2(NAME) #NAME
#define SEEC__STR(NAME) SEEC__STR2(NAME)

#define SEEC_INTERCEPTED_FUNCTION(NAME)                                        \
  if (auto Ptr = dlsym(RTLD_DEFAULT, SEEC__STR(SEEC_MANGLE_FUNCTION(NAME))))   \
    InterceptorAddresses.insert(reinterpret_cast<uintptr_t>(Ptr));
#include "seec/Runtimes/Tracer/InterceptedFunctions.def"

#undef SEEC__STR2
#undef SEEC__STR
}

ProcessEnvironment::~ProcessEnvironment()
{
  ThreadLookup.clear();
  ProcessTracer.reset();
}


//------------------------------------------------------------------------------
// getProcessEnvironment()
//------------------------------------------------------------------------------

ProcessEnvironment &getProcessEnvironment() {
  static bool Initialized = false;
  static std::mutex InitializationMutex {};
  static std::unique_ptr<ProcessEnvironment> ProcessEnv {};
  
  if (Initialized)
    return *ProcessEnv;
  
  std::lock_guard<std::mutex> Lock(InitializationMutex);
  if (Initialized)
    return *ProcessEnv;
  
  ProcessEnv.reset(new ProcessEnvironment());
  Initialized = true;
  
  return *ProcessEnv;
}


//------------------------------------------------------------------------------
// getThreadEnvironment()
//------------------------------------------------------------------------------

ThreadEnvironment &getThreadEnvironment() {
  // Yes, this is a terrible bottleneck. When thread_local support is available,
  // we'll be using that instead.
  static std::mutex AccessMutex {};
  
  std::lock_guard<std::mutex> Lock(AccessMutex);
  
  auto &Lookup = getProcessEnvironment().getThreadLookup();
  auto &ThreadEnvPtr = Lookup[std::this_thread::get_id()];
  
  if (!ThreadEnvPtr)
    ThreadEnvPtr.reset(new ThreadEnvironment(getProcessEnvironment()));
  
  return *ThreadEnvPtr;
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

void SeeCRecordFunctionEnd(uint32_t Index) {
  // auto &ModIndex = Environment::getModuleIndex();
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto F = ThreadEnv.popFunction();
  // Check F matches Index?
  Listener.notifyFunctionEnd(Index, F);
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

void SeeCLockDynamicMemory() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.acquireDynamicMemoryLock();
}

void SeeCRecordMalloc(void const * const Address, size_t const Size) {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.recordMalloc(reinterpret_cast<uintptr_t>(Address), Size);
}

void SeeCRecordFree(void const * const Address) {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.recordFreeAndClear(reinterpret_cast<uintptr_t>(Address));
}

void SeeCLockMemoryForWriting() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.acquireGlobalMemoryWriteLock();
}

void SeeCLockMemoryForReading() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.acquireGlobalMemoryReadLock();
}

void SeeCRecordUntypedState(char const * const Data, size_t const Size) {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.recordUntypedState(Data, Size);
}

void SeeCReleaseLocks() {
  auto &Listener = seec::trace::getThreadEnvironment().getThreadListener();
  Listener.exitPostNotification();
}

void SeeCCheckCStringRead(char const * const CString) {
  using namespace seec::runtime_errors::format_selects;
  
  auto &Env = seec::trace::getThreadEnvironment();
  
  seec::trace::CStdLibChecker Checker {Env.getThreadListener(),
                                       Env.getInstructionIndex(),
                                       CStdFunction::userdefined};
  
  Checker.checkCStringRead(1, CString);
}

} // extern "C"
