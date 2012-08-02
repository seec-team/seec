#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceStorage.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Type.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Threading.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
#include <thread>

#include <signal.h>

#include "seec/Transforms/RecordExternal/RecordInfo.h" // needs <cstdint>

using namespace llvm;
using namespace seec;
using namespace seec::trace;

namespace seec {

namespace trace {

// Forward-declaration.
class ProcessEnvironment;


/// \brief ThreadEnvironment.
///
class ThreadEnvironment {
  ProcessEnvironment &Process;
  
  TraceThreadListener ThreadTracer;
  
  FunctionIndex *FunIndex;
  
  std::vector<Function *> Stack;
  
public:
  ThreadEnvironment(ProcessEnvironment &PE);
  
  ~ThreadEnvironment() {
    llvm::errs() << "~ThreadEnvironment\n";
  }
  
  TraceThreadListener &getThreadListener() { return ThreadTracer; }
  
  FunctionIndex &getFunctionIndex() {
    assert(FunIndex);
    return *FunIndex;
  }
  
  void pushFunction(llvm::Function *Fun);
  
  llvm::Function *popFunction();
};


/// \brief ProcessEnvironment.
///
class ProcessEnvironment {
  llvm::LLVMContext Context;
  
  std::unique_ptr<llvm::Module> Mod;
  
  std::unique_ptr<ModuleIndex> ModIndex;
  
  OutputStreamAllocator StreamAllocator;
  
  seec::SynchronizedExit SyncExit;
  
  std::unique_ptr<TraceProcessListener> ProcessTracer;
  
  std::map<std::thread::id, std::unique_ptr<ThreadEnvironment>> ThreadLookup;
  
public:
  ProcessEnvironment()
  : Context(),
    Mod(),
    ModIndex(),
    StreamAllocator(),
    SyncExit(),
    ProcessTracer(),
    ThreadLookup()
  {
    llvm::llvm_start_multithreaded();
    
    // Load Mod
    SMDiagnostic Err;
    Mod.reset(ParseIRFile(SeeCInfoModuleIdentifier, Err, Context));
    
    if (!Mod) {
      llvm::errs() << "\nFailed to load module '" 
                   << SeeCInfoModuleIdentifier
                   << "'\n";
      exit(EXIT_FAILURE);
    }
    
    // Build ModIndex
    ModIndex.reset(new ModuleIndex(*Mod));
    
    // Create the process tracer
    ProcessTracer.reset(new TraceProcessListener(*Mod, 
                                                 *ModIndex, 
                                                 StreamAllocator,
                                                 SyncExit));

    // Give the listener the run-time locations of functions
    uint32_t FunIndex = 0;
    for (auto &Fun: *Mod) {
      if (Fun.isIntrinsic())
        continue;

      ProcessTracer->notifyFunction(FunIndex,
                                    &Fun,
                                    SeeCInfoFunctions[FunIndex]);
      
      ++FunIndex;
    }

    // Give the listener the run-time locations of globals
    uint32_t GlobalIndex = 0;
    for (auto GlobalIt = Mod->global_begin(), GlobalEnd = Mod->global_end();
         GlobalIt != GlobalEnd; ++GlobalIt) {
      ProcessTracer->notifyGlobalVariable(GlobalIndex,
                                          &*GlobalIt,
                                          SeeCInfoGlobals[GlobalIndex]);
      ++GlobalIndex;
    }
  }
  
  ~ProcessEnvironment() {
    llvm::errs() << "~ProcessEnvironment\n";
  }
  
  decltype(ThreadLookup) &getThreadLookup() { return ThreadLookup; }
  
  llvm::LLVMContext &getContext() { return Context; }
  
  llvm::Module const &getModule() const { return *Mod; }
  
  ModuleIndex &getModuleIndex() { return *ModIndex; }
  
  OutputStreamAllocator &getStreamAllocator() { return StreamAllocator; }
  
  SynchronizedExit &getSynchronizedExit() { return SyncExit; }
  
  TraceProcessListener &getProcessListener() { return *ProcessTracer; }
};


//------------------------------------------------------------------------------
// ThreadEnvironment
//------------------------------------------------------------------------------

ThreadEnvironment::ThreadEnvironment(ProcessEnvironment &PE)
: Process(PE),
  ThreadTracer(PE.getProcessListener(), PE.getStreamAllocator()),
  FunIndex(nullptr),
  Stack()
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
  auto &ThreadEnv = getThreadEnvironment();
  auto &ModIndex = getProcessEnvironment().getModuleIndex();
  auto &Listener = ThreadEnv.getThreadListener();
  auto F = ModIndex.getFunction(Index);
  Listener.notifyFunctionBegin(Index, F);
  ThreadEnv.pushFunction(F);
}

void SeeCRecordFunctionEnd(uint32_t Index) {
  // auto &ModIndex = Environment::getModuleIndex();
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto F = ThreadEnv.popFunction();
  // Check F matches Index?
  Listener.notifyFunctionEnd(Index, F);
}

void SeeCRecordPreLoad(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Load = dyn_cast<LoadInst>(Inst);
  assert(Load && "Expected LoadInst");

  Listener.notifyPreLoad(Index, Load, Address, Size);
}

void SeeCRecordPostLoad(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Load = dyn_cast<LoadInst>(Inst);
  assert(Load && "Expected LoadInst");

  Listener.notifyPostLoad(Index, Load, Address, Size);
}

void SeeCRecordPreStore(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Store = dyn_cast<StoreInst>(Inst);
  assert(Store && "Expected StoreInst");

  Listener.notifyPreStore(Index, Store, Address, Size);
}

void SeeCRecordPostStore(uint32_t Index, void *Address, uint64_t Size) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Store = dyn_cast<StoreInst>(Inst);
  assert(Store && "Expected StoreInst");

  Listener.notifyPostStore(Index, Store, Address, Size);
}

void SeeCRecordPreCall(uint32_t Index, void *Address) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Call = dyn_cast<CallInst>(Inst);
  assert(Call && "Expected CallInst");

  Listener.notifyPreCall(Index, Call, Address);
}

void SeeCRecordPostCall(uint32_t Index, void *Address) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Call = dyn_cast<CallInst>(Inst);
  assert(Call && "Expected CallInst");

  Listener.notifyPostCall(Index, Call, Address);
}

void SeeCRecordPreCallIntrinsic(uint32_t Index) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Call = dyn_cast<CallInst>(Inst);
  assert(Call && "Expected CallInst");

  Listener.notifyPreCallIntrinsic(Index, Call);
}

void SeeCRecordPostCallIntrinsic(uint32_t Index) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");

  auto Call = dyn_cast<CallInst>(Inst);
  assert(Call && "Expected CallInst");

  Listener.notifyPostCallIntrinsic(Index, Call);
}

void SeeCRecordPreDivide(uint32_t Index) {
  auto &ThreadEnv = getThreadEnvironment();
  auto &FunIndex = ThreadEnv.getFunctionIndex();
  auto &Listener = ThreadEnv.getThreadListener();

  auto Inst = FunIndex.getInstruction(Index);
  assert(Inst && "Bad Index");
  
  auto BinOp = dyn_cast<BinaryOperator>(Inst);
  assert(BinOp && "Expected BinaryOperator");

  Listener.notifyPreDivide(Index, BinOp);
}

#define SEEC_RECORD_UPDATE(NAME, TYPE)                                         \
void SeeCRecordUpdate##NAME(uint32_t Index, TYPE Value) {                      \
  auto &ThreadEnv = getThreadEnvironment();                                    \
  auto &FunIndex = ThreadEnv.getFunctionIndex();                               \
  auto &Listener = ThreadEnv.getThreadListener();                              \
  auto Inst = FunIndex.getInstruction(Index);                                  \
  assert(Inst && "Bad Index");                                                 \
  Listener.notifyValue(Index, Inst, Value);                                    \
}

SEEC_RECORD_UPDATE(Pointer, void *)
SEEC_RECORD_UPDATE(Int64, uint64_t)
SEEC_RECORD_UPDATE(Int32, uint32_t)
SEEC_RECORD_UPDATE(Int16, uint16_t)
SEEC_RECORD_UPDATE(Int8, uint8_t)
SEEC_RECORD_UPDATE(Float, float)
SEEC_RECORD_UPDATE(Double, double)

#undef SEEC_RECORD_UPDATE

} // extern "C"
