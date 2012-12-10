//===- RecordExternal.hpp - Insert execution tracing ---------------- C++ -===//
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

#ifndef SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDEXTERNAL_HPP
#define SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDEXTERNAL_HPP

#include "seec/Util/ModuleIndex.hpp"

#include "llvm/DataLayout.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/InstVisitor.h"

#include <memory>
#include <vector>

namespace llvm {

class DataLayout;

/// This pass inserts calls to external execution tracing functions.
class InsertExternalRecording
: public FunctionPass,
  public InstVisitor<InsertExternalRecording>
{
private:
  /// \name Members.
  /// @{
  
#define HANDLE_RECORD_POINT(POINT, LLVM_FUNCTION_TYPE) \
  Function *Record##POINT;
#include "seec/Transforms/RecordExternal/RecordPoints.def"

  /// Map from standard functions to the SeeC interceptors.
  llvm::DenseMap<llvm::Function *, llvm::Function *> FunctionInterceptions;

  /// Original Instructions of the current Function
  std::vector<Instruction *> FunctionInstructions;

  /// Index of instruction currently being instrumented
  uint32_t InstructionIndex;

  Type *Int32Ty;   ///< Type of i32.
  Type *Int64Ty;   ///< Type of i64.
  Type *Int8PtrTy; ///< Type of i8 *.

  /// DataLayout for the Module.
  DataLayout *DL;

  /// Index of the Module.
  std::unique_ptr<seec::ModuleIndex> ModIndex;
  
  /// @}
  

  /// \name Helper methods.
  /// @{
  
  /// \brief Add a prototype for an interceptor function.
  ///
  llvm::Function *createFunctionInterceptorPrototype(llvm::Function *ForFn,
                                                     llvm::StringRef NewName);
  
  /// \brief Insert a call to update an Instruction's runtime value.
  ///
  CallInst *insertRecordUpdateForValue(Instruction &I,
                                       Instruction *Before = nullptr);

  /// @}
  
public:
  static char ID; ///< For LLVM's RTTI

  /// Constructor.
  /// \param Listener the InternalRecordingListener that will be notified of
  ///        events during execution of the instrumented Module.
  InsertExternalRecording()
  : FunctionPass(ID),
    FunctionInterceptions(),
    FunctionInstructions(),
    InstructionIndex(),
    Int32Ty(nullptr),
    Int64Ty(nullptr),
    Int8PtrTy(nullptr),
    DL(nullptr),
    ModIndex(nullptr)
  {}

  /// Get a string containing the name of this pass.
  /// \return A string containing the name of this pass.
  const char *getPassName() const {
    return "Insert SeeC External Execution Tracing";
  }

  virtual bool doInitialization(Module &M);

  virtual bool runOnFunction(Function &F);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  /// \name InstVisitor methods
  /// @{
#define SIMPLE_RECORD_UPDATE_FOR_VALUE(INST_TYPE) \
  void visit##INST_TYPE(INST_TYPE &I) { insertRecordUpdateForValue(I); }

  void visitBinaryOperator(BinaryOperator &I);

  SIMPLE_RECORD_UPDATE_FOR_VALUE(CmpInst)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(CastInst)

  // Terminator instructions
  void visitReturnInst(ReturnInst &I);

  // Memory operators
  void visitAllocaInst(AllocaInst &I);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);
  SIMPLE_RECORD_UPDATE_FOR_VALUE(GetElementPtrInst)

  // Other operators
  SIMPLE_RECORD_UPDATE_FOR_VALUE(PHINode)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(SelectInst)
  void visitCallInst(CallInst &I);

#undef SIMPLE_RECORD_UPDATE_FOR_VALUE
  /// @} (InstVisitor methods)
};

}

#endif // SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDEXTERNAL_HPP
