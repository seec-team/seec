//===- RecordInternal.hpp - Insert execution tracing ---------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRANSFORMS_RECORDINTERNAL_RECORDINTERNAL_HPP
#define SEEC_TRANSFORMS_RECORDINTERNAL_RECORDINTERNAL_HPP

#include "seec/Trace/ExecutionListener.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/Pass.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetData.h"

#include <memory>
#include <vector>
#include <csetjmp>

namespace seec {

/// Map notifications to the original Module and forward them to a Listener.

/// Takes basic notifications from an executing instrumented Module, maps them
/// to the original Module. This works with InsertInternalRecording so that one
/// can instrument a Module, JIT it, execute it, and receive information about
/// the execution process with reference to the original Module (as it was prior
/// to instrumentation). This class forwards the mapped information to an
/// ExecutionListener.

class InternalRecordingListener {
private:
  /// Original, uninstrumented copy of the module.
  llvm::Module *OriginalModule;

  /// Used to lookup items by index.
  ModuleIndex OriginalModuleIndex;

  /// TargetData used when instrumenting the module.
  llvm::TargetData *TD;

  /// Current stack of instrumented functions.
  std::vector<llvm::Function *> CallStack;

  /// The general listener that we will pass information to.
  trace::ExecutionListener *Listener;

  /// Holds the value passed to exit() by the instrumented Module.
  int ExitCode;

public:
  /// Point to longjmp() to if the instrumented Module calls exit().
  jmp_buf ExitJump;

  /// Constructor.
  /// \param OriginalModule a copy of the original, uninstrumented Module.
  /// \param TD the TargetData used when instrumenting the Module.
  /// \param Listener the ExecutionListener that we will forward mapped
  ///        execution information to.
  InternalRecordingListener(llvm::Module *OriginalModule,
                            llvm::TargetData *TD,
                            trace::ExecutionListener *Listener)
  : OriginalModule(OriginalModule),
    OriginalModuleIndex(*OriginalModule),
    TD(TD),
    CallStack(),
    Listener(Listener),
    ExitCode(0)
  {}

  /// Get the uninstrumented Module.
  /// \return The copy of the original, uninstrumented Module.
  llvm::Module *getModule() { return OriginalModule; }

  /// Get the TargetData for the Module.
  /// \return the TargetData used when instrumenting the Module.
  llvm::TargetData *getTargetData() { return TD; }

  /// Get the ExecutionListener that we are forwarding information to.
  /// \return an ExecutionListener.
  trace::ExecutionListener *getListener() { return Listener; }

  /// Get the value passed to exit() by the instrumented Module.
  /// \return the int value passed to exit(), if exit() has been called,
  ///         otherwise undefined.
  int getExitCode() const { return ExitCode; }

  /// Get the Instruction at a given index in the current Function.
  /// \param InstructionIndex the index to find.
  /// \return the Instruction at the given index, if it exists, else nullptr.
  llvm::Instruction *getInstruction(uint32_t InstructionIndex);

  void recordFunctionBegin(llvm::Function *F);
  void recordFunctionEnd();

  void recordPreCall(uint32_t InstructionIndex, void *Address);
  void recordPostCall(uint32_t InstructionIndex, void *Address);

  void recordPreCallIntrinsic(uint32_t InstructionIndex);
  void recordPostCallIntrinsic(uint32_t InstructionIndex);

  void recordLoad(uint32_t InstructionIndex, void *Address, uint64_t Length);
  void recordPreStore(uint32_t InstructionIndex, void *Address,
                      uint64_t Length);
  void recordPostStore(uint32_t InstructionIndex, void *Address,
                       uint64_t Length);

  void recordUpdatePointer(uint32_t InstructionIndex, void *Value);

  /// Receive a new value for an integer with 8 or less bits.
  /// \param InstructionIndex index of the Instruction whose value is being
  ///        updated.
  /// \param Value the updated value for the Instruction, zero-extended if
  ///        necessary.
  void recordUpdateInt8(uint32_t InstructionIndex, uint8_t Value) {
    recordUpdateInt64(InstructionIndex, Value);
  }

  /// Receive a new value for an integer with 16 or less bits.
  /// \param InstructionIndex index of the Instruction whose value is being
  ///        updated.
  /// \param Value the updated value for the Instruction, zero-extended if
  ///        necessary.
  void recordUpdateInt16(uint32_t InstructionIndex, uint16_t Value) {
    recordUpdateInt64(InstructionIndex, Value);
  }

  /// Receive a new value for an integer with 32 or less bits.
  /// \param InstructionIndex index of the Instruction whose value is being
  ///        updated.
  /// \param Value the updated value for the Instruction, zero-extended if
  ///        necessary.
  void recordUpdateInt32(uint32_t InstructionIndex, uint32_t Value) {
    recordUpdateInt64(InstructionIndex, Value);
  }

  void recordUpdateInt64(uint32_t InstructionIndex, uint64_t Value);
  void recordUpdateFloat(uint32_t InstructionIndex, float Value);
  void recordUpdateDouble(uint32_t InstructionIndex, double Value);

  /// Receives instrumented calls to exit().
  /// \param Code the exit code passed to exit().
  void redirect_exit(int Code);

  /// Receives instrumented calls to atexit().
  int redirect_atexit(void (*function)(void));
};

}

namespace llvm {

class TargetData;

/// This pass inserts calls to external execution tracing functions.
class InsertInternalRecording :
  public FunctionPass, public InstVisitor<InsertInternalRecording> {
private:
  /// The recording listener that will be used by the instrumented module
  seec::InternalRecordingListener *Listener;

  /// A ConstantInt holding the address of the recording listener.
  ConstantInt *ListenerAddress;

  /// Copy of the original, uninstrumented module
  Module *OriginalModule;

#define HANDLE_RECORD_POINT(POINT, LLVM_FUNCTION_TYPE) \
  Function *Record##POINT;
#include "seec/Transforms/RecordInternal/RecordPoints.def"

  /// Index of function currently being instrumented
  uint32_t FunctionIndex;

  /// Original Instructions of the current Function
  std::vector<Instruction *> FunctionInstructions;

  /// Index of instruction currently being instrumented
  uint32_t InstructionIndex;

  Type *Int32Ty;

  TargetData *TD;

  CallInst *insertRecordUpdateForValue(Instruction &I,
                                       Instruction *Before = nullptr);

public:
  static char ID; ///< For LLVM's RTTI

  /// Constructor.
  /// \param Listener the InternalRecordingListener that will be notified of
  ///        events during execution of the instrumented Module.
  InsertInternalRecording(seec::InternalRecordingListener *Listener)
  : FunctionPass(ID),
    Listener(Listener),
    OriginalModule(Listener->getModule())
  {}

  /// Get a string containing the name of this pass.
  /// \return A string containing the name of this pass.
  const char *getPassName() const {
    return "Insert SeeC Internal Execution Tracing";
  }

  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  // InstVisitor methods
#define SIMPLE_RECORD_UPDATE_FOR_VALUE(INST_TYPE) \
  void visit##INST_TYPE(INST_TYPE &I) { insertRecordUpdateForValue(I); }

  SIMPLE_RECORD_UPDATE_FOR_VALUE(BinaryOperator)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(CmpInst)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(CastInst)

  // Terminator instructions
  void visitReturnInst(ReturnInst &I);

  // Memory operators
  void visitAllocaInst(AllocaInst &I);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);

  // Other operators
  SIMPLE_RECORD_UPDATE_FOR_VALUE(PHINode)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(SelectInst)
  void visitCallInst(CallInst &I);

#undef SIMPLE_RECORD_UPDATE_FOR_VALUE
};

}

#endif // SEEC_TRANSFORMS_RECORDINTERNAL_RECORDINTERNAL_HPP
