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
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Type.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Threading.h"

#include <cassert>
#include <cstdint>
#include <thread>

#include <dlfcn.h>

#include "seec/Transforms/RecordExternal/RecordInfo.h" // needs <cstdint>


namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// ThreadEnvironment
//------------------------------------------------------------------------------

ThreadEnvironment::ThreadEnvironment(ProcessEnvironment &PE)
: Process(PE),
  ThreadTracer(PE.getProcessListener(), PE.getStreamAllocator()),
  FunIndex(nullptr),
  Stack(),
  InstructionIndex(0)
{}

void ThreadEnvironment::pushFunction(llvm::Function *Fun) {
  Stack.push_back(Fun);
  FunIndex = Process.getModuleIndex().getFunctionIndex(Fun);
}

llvm::Function *ThreadEnvironment::popFunction() {
  auto Fun = Stack.back();
  Stack.pop_back();
  
  FunIndex = 
  Stack.empty() ? nullptr
  : Process.getModuleIndex().getFunctionIndex(Stack.back());
  
  return Fun;
}

llvm::Instruction *ThreadEnvironment::getInstruction() const {
  return getFunctionIndex().getInstruction(InstructionIndex);
}


//------------------------------------------------------------------------------
// ThreadEnvironment
//------------------------------------------------------------------------------

ProcessEnvironment::ProcessEnvironment()
: Context(),
  Mod(),
  ModIndex(),
  StreamAllocator(),
  SyncExit(),
  ProcessTracer(),
  ThreadLookup(),
  InterceptorAddresses()
{
  llvm::llvm_start_multithreaded();
  
  // Load Mod.
  llvm::SMDiagnostic Err;
  Mod.reset(llvm::ParseIRFile(SeeCInfoModuleIdentifier, Err, Context));
  
  if (!Mod) {
    llvm::errs() << "\nFailed to load module '" 
                 << SeeCInfoModuleIdentifier
                 << "'\n";
    exit(EXIT_FAILURE);
  }
  
  // Build ModIndex.
  ModIndex.reset(new ModuleIndex(*Mod));
  
  // Create the process tracer.
  ProcessTracer.reset(new TraceProcessListener(*Mod, 
                                               *ModIndex, 
                                               StreamAllocator,
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

#define SEEC_RECORD_UPDATE(NAME, TYPE)                                         \
void SeeCRecordUpdate##NAME(uint32_t Index, TYPE Value) {                      \
  auto &ThreadEnv = seec::trace::getThreadEnvironment();                       \
  ThreadEnv.setInstructionIndex(Index);                                        \
  if (ThreadEnv.getInstructionIsInterceptedCall())                             \
    return;                                                                    \
  auto &Listener = ThreadEnv.getThreadListener();                              \
  Listener.notifyValue(Index, ThreadEnv.getInstruction(), Value);              \
}

SEEC_RECORD_UPDATE(Pointer, void *)
SEEC_RECORD_UPDATE(Int64,   uint64_t)
SEEC_RECORD_UPDATE(Int32,   uint32_t)
SEEC_RECORD_UPDATE(Int16,   uint16_t)
SEEC_RECORD_UPDATE(Int8,    uint8_t)
SEEC_RECORD_UPDATE(Float,   float)
SEEC_RECORD_UPDATE(Double,  double)
SEEC_RECORD_UPDATE(X86FP80, long double)

// UpdateFP128
// UpdatePPC128

#undef SEEC_RECORD_UPDATE

} // extern "C"
