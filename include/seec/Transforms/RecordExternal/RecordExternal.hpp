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

#include "llvm/Pass.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/DataTypes.h"

#include <memory>
#include <string>
#include <vector>


namespace llvm {


class DataLayout;


/// \brief Inserts calls to external execution tracing functions.
///
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

  /// Path to SeeC resources.
  std::string const ResourcePath;

  /// Set of all SeeC interceptor functions used by this Module.
  llvm::DenseMap<llvm::Function *, llvm::Function *> Interceptors;
  
  /// Original Instructions of the current Function
  std::vector<Instruction *> FunctionInstructions;

  /// Index of instruction currently being instrumented
  uint32_t InstructionIndex;

  Type *Int32Ty;   ///< Type of i32.
  Type *Int64Ty;   ///< Type of i64.
  Type *Int8PtrTy; ///< Type of i8 *.

  /// DataLayout for the Module.
  std::unique_ptr<DataLayout> DL;

  /// Index of the Module.
  std::unique_ptr<seec::ModuleIndex> ModIndex;
  
  /// All unhandled external functions.
  llvm::SmallPtrSet<llvm::Function *, 16> UnhandledFunctions;
  
  /// @} (Members.)
  

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

  /// @} (Helper methods.)
  
public:
  static char ID; ///< For LLVM's RTTI

  /// \brief Constructor.
  /// \param PathToSeeCResources path to SeeC resources.
  ///
  InsertExternalRecording(llvm::StringRef PathToSeeCResources)
  : FunctionPass(ID),
    ResourcePath(PathToSeeCResources),
    Interceptors(),
    FunctionInstructions(),
    InstructionIndex(),
    Int32Ty(nullptr),
    Int64Ty(nullptr),
    Int8PtrTy(nullptr),
    DL(nullptr),
    ModIndex(nullptr),
    UnhandledFunctions()
  {}

  /// \brief Get a string containing the name of this pass.
  /// \return A string containing the name of this pass.
  ///
  virtual llvm::StringRef getPassName() const override {
    return "Insert SeeC External Execution Tracing";
  }

  virtual bool doInitialization(Module &M) override;

  /// \brief Instrument a single function.
  ///
  /// \param F the function to instrument.
  /// \return true if the function was modified.
  ///
  virtual bool runOnFunction(Function &F) override;

  /// \brief Determine whether or not this pass will invalidate any analyses.
  ///
  virtual void getAnalysisUsage(AnalysisUsage &AU) const override;
  
  /// \brief Get all encountered unhandled functions.
  ///
  decltype(UnhandledFunctions) const &getUnhandledFunctions() const {
    return UnhandledFunctions;
  }


  /// \name InstVisitor methods.
  /// @{
  
#define SIMPLE_RECORD_UPDATE_FOR_VALUE(INST_TYPE) \
  void visit##INST_TYPE(INST_TYPE &I) { insertRecordUpdateForValue(I); }

  void visitBinaryOperator(BinaryOperator &I);

  SIMPLE_RECORD_UPDATE_FOR_VALUE(CmpInst)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(CastInst)
  SIMPLE_RECORD_UPDATE_FOR_VALUE(ExtractElementInst)

  // Terminator instructions
  void visitReturnInst(ReturnInst &I);

  // Memory operators
  void visitAllocaInst(AllocaInst &I);
  void visitLoadInst(LoadInst &LI);
  void visitStoreInst(StoreInst &SI);
  SIMPLE_RECORD_UPDATE_FOR_VALUE(GetElementPtrInst)

  // Other operators
  void visitPHINode(PHINode &I);
  SIMPLE_RECORD_UPDATE_FOR_VALUE(SelectInst)
  void visitCallInst(CallInst &I);
  
  SIMPLE_RECORD_UPDATE_FOR_VALUE(UnaryInstruction)

#undef SIMPLE_RECORD_UPDATE_FOR_VALUE
  
  /// @} (InstVisitor methods.)
};


}

#endif // SEEC_TRANSFORMS_RECORDEXTERNAL_RECORDEXTERNAL_HPP
