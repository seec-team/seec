//===- lib/Runtimes/Tracer/Tracer.hpp -------------------------------------===//
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

#ifndef SEEC_LIB_RUNTIMES_TRACER_TRACER_HPP
#define SEEC_LIB_RUNTIMES_TRACER_TRACER_HPP


#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/IndexTypesForLLVMObjects.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/DenseSet.h"

#include <cassert>
#include <memory>
#include <mutex>
#include <vector>


namespace seec {

class AugmentationCollection;
class ResourceLoader;

namespace trace {

// Forward-declaration.
class ProcessEnvironment;


struct FunctionEnvironment {
  llvm::Function *Function;
  
  InstrIndexInFn InstructionIndex;
  
  /// Is the current instruction an intercepted call?
  bool InstructionIsInterceptedCall;
  
  FunctionEnvironment(llvm::Function *Function)
  : Function(Function),
    InstructionIndex(0u),
    InstructionIsInterceptedCall(false)
  {}
};


/// \brief ThreadEnvironment.
///
class ThreadEnvironment {
  /// The shared process environment.
  ProcessEnvironment &Process;
  
  /// This thread's listener.
  TraceThreadListener ThreadTracer;
  
  /// The index for the currently active function.
  FunctionIndex *FunIndex;
  
  /// The call stack of functions.
  std::vector<FunctionEnvironment> Stack;
  
public:
  /// \brief Constructor.
  ///
  ThreadEnvironment(ProcessEnvironment &PE);
  
  /// \brief Destructor.
  ///
  ~ThreadEnvironment();
  
  /// \brief Get this thread's listener.
  ///
  TraceThreadListener &getThreadListener() { return ThreadTracer; }
  
  
  /// \name Accessors.
  /// @{
  
  ProcessEnvironment &getProcessEnvironment() { return Process; }
  
  /// @} (Accessors.)


  /// \name Trace output.
  /// @{

  /// \brief Check if this thread's output is too large, in which case the
  ///        tracing will be stopped (but execution will continue).
  ///
  void checkOutputSize();

  /// @}
  
  
  /// \name Function tracking.
  /// @{
  
  /// \brief Get the index for the currently active llvm::Function.
  ///
  FunctionIndex &getFunctionIndex() const {
    assert(FunIndex);
    return *FunIndex;
  }
  
  /// \brief Push a new llvm::Function onto the call stack.
  ///
  void pushFunction(llvm::Function *Fun);
  
  /// \brief Pop the last llvm::Function from the call stack.
  ///
  llvm::Function *popFunction();
  
  /// @}
  
  
  /// \name Instruction tracking.
  /// @{
  
  /// \brief Set the current instruction index.
  ///
  void setInstructionIndex(InstrIndexInFn Value) {
    // If the instruction index is the same then avoid resetting the value of
    // InstructionIsInterceptedCall, because this is happening in a value
    // update notification, but that notification depends on the correct value
    // of InstructionIsInterceptedCall.
    if (Stack.back().InstructionIndex == Value)
      return;
    
    Stack.back().InstructionIndex = Value;
    Stack.back().InstructionIsInterceptedCall = false;
  }
  
  /// \brief Get the current instruction index.
  ///
  InstrIndexInFn getInstructionIndex() const {
    return Stack.back().InstructionIndex;
  }
  
  /// \brief Get the current llvm::Instruction.
  ///
  llvm::Instruction *getInstruction() const;
  
  /// \brief Set the current instruction to be an intercepted call.
  ///
  void setInstructionIsInterceptedCall() {
    Stack.back().InstructionIsInterceptedCall = true;
  }
  
  /// \brief Check if the current instruction is an intercepted call.
  ///
  bool getInstructionIsInterceptedCall() const {
    return Stack.back().InstructionIsInterceptedCall;
  }
  
  /// @}
};


/// \brief ProcessEnvironment.
///
class ProcessEnvironment {
  /// Context for the original Module.
  llvm::LLVMContext Context;
  
  /// The original Module.
  std::unique_ptr<llvm::Module> Mod;
  
  /// Indexed view of the original Module.
  std::unique_ptr<ModuleIndex> ModIndex;
  
  /// Allocator for the trace's output streams.
  std::unique_ptr<OutputStreamAllocator> StreamAllocator;
  
  /// Loads ICU resources.
  std::unique_ptr<ResourceLoader> ICUResourceLoader;

  /// Process listener.
  std::unique_ptr<TraceProcessListener> ProcessTracer;
  
  /// Thread environments.
  std::map<std::thread::id, std::unique_ptr<ThreadEnvironment>> ThreadLookup;
  
  /// Controls access to the thread environments.
  std::mutex ThreadLookupMutex;
  
  /// Interceptor function addresses.
  llvm::DenseSet<uintptr_t> InterceptorAddresses;
  
  /// Size limit for trace files.
  offset_uint TraceSizeLimit;
  
  /// The program name as found in argv[0], if we were notified of it.
  std::string ProgramName;
  
public:
  /// \brief Constructor.
  ///
  ProcessEnvironment();
  
  /// \brief Destructor.
  ///
  /// Ensures that ThreadEnvironment objects, and thus ThreadListener objects,
  /// are destroyed before the shared ProcessListener object.
  ///
  ~ProcessEnvironment();
  
  llvm::LLVMContext &getContext() { return Context; }
  
  llvm::Module const &getModule() const { return *Mod; }
  
  ModuleIndex &getModuleIndex() { return *ModIndex; }
  
  OutputStreamAllocator &getStreamAllocator() { return *StreamAllocator; }
  
  TraceProcessListener &getProcessListener() { return *ProcessTracer; }
  
  
  /// \name Intercepted function detection.
  /// @{
  
  bool isInterceptedFunction(uintptr_t Address) const {
    return InterceptorAddresses.count(Address);
  }
  
  /// @}
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the size limit for trace files.
  ///
  offset_uint getTraceSizeLimit() const { return TraceSizeLimit; }
  
  /// \brief Get the program name as found in argv[0] (may be empty).
  ///
  std::string const &getProgramName() const { return ProgramName; }
  
  /// @}
  
  
  /// \name Mutators.
  /// @{
  
  /// \brief Get the current thread's environment.
  ///
  ThreadEnvironment *getOrCreateCurrentThreadEnvironment();
  
  /// \brief Set the program name as found in argv[0].
  ///
  void setProgramName(llvm::StringRef Name);
  
  /// @}
};


/// \brief Get the global ProcessEnvironment.
///
ProcessEnvironment &getProcessEnvironment();


/// \brief Get the current thread's ThreadEnvironment.
///
ThreadEnvironment &getThreadEnvironment();


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_LIB_RUNTIMES_TRACER_TRACER_HPP
