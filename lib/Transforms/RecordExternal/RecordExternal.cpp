//===- lib/Transforms/RecordExternal.cpp ----------------------------------===//
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

#define DEBUG_TYPE "seec"

#include "seec/Runtimes/MangleFunction.h"
#include "seec/Transforms/RecordExternal/RecordExternal.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Type.h"
#include "llvm/TypeBuilder.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/DataLayout.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <vector>
#include <cassert>

namespace llvm {

char InsertExternalRecording::ID = 0;

llvm::Function *
InsertExternalRecording::
createFunctionInterceptorPrototype(llvm::Function *ForFn,
                                   llvm::StringRef NewName)
{
  auto ForFnType = ForFn->getFunctionType();
  auto NumParams = ForFnType->getNumParams();
  
  llvm::SmallVector<llvm::Type *, 10> ParamTypes;
  
  ParamTypes.push_back(Int32Ty);
  
  for (unsigned i = 0; i < NumParams; ++i)
    ParamTypes.push_back(ForFnType->getParamType(i));
  
  auto NewFnType = FunctionType::get(ForFnType->getReturnType(),
                                     ParamTypes,
                                     ForFnType->isVarArg());
  
  auto Mod = ForFn->getParent();
  if (auto ExistingFn = Mod->getFunction(NewName)) {
    // TODO: Check type.
    return ExistingFn;
  }
  
  auto Attributes = ForFn->getAttributes();
  auto NewFn = Mod->getOrInsertFunction(NewName, NewFnType, Attributes);
  
  return dyn_cast<Function>(NewFn);
}

/// Insert a call to notify SeeC of the new run-time value of I.
/// \param I the Instruction whose new run-time value is being recorded.
/// \return The Instruction which calls the notification function.
///
CallInst *
InsertExternalRecording::insertRecordUpdateForValue(Instruction &I,
                                                    Instruction *Before) {
  LLVMContext &Context = I.getContext();
  Type const *Ty = I.getType();

  Instruction *CastInstr = nullptr;

  Function *RecordFn = nullptr;

  // Get the recording function
  if (IntegerType const *IntTy = dyn_cast<IntegerType>(Ty)) {
    uint32_t BitWidth = IntTy->getBitWidth();

#define HANDLE_WIDTH_BELOW_OR_EQUAL(WIDTH) \
    if (BitWidth <= WIDTH) { \
      RecordFn = RecordUpdateInt ## WIDTH; \
      if (BitWidth != WIDTH) { \
        CastInstr = new ZExtInst(&I, Type::getInt ## WIDTH ## Ty(Context)); \
      } \
    }

    HANDLE_WIDTH_BELOW_OR_EQUAL(8)
    else HANDLE_WIDTH_BELOW_OR_EQUAL(16)
    else HANDLE_WIDTH_BELOW_OR_EQUAL(32)
    else HANDLE_WIDTH_BELOW_OR_EQUAL(64)
    else {
      return nullptr;
    }

#undef HANDLE_WIDTH_BELOW_OR_EQUAL
  }
  else if (isa<PointerType>(Ty)) {
    RecordFn = RecordUpdatePointer;

    // If the pointer isn't an i8*, we need to bitcast it to one
    if (Ty != Int8PtrTy) {
      CastInstr = new BitCastInst(&I, Int8PtrTy);
    }
  }
  else if (Ty->isFloatTy())
    RecordFn = RecordUpdateFloat;
  else if (Ty->isDoubleTy())
    RecordFn = RecordUpdateDouble;
  else if (Ty->isX86_FP80Ty())
    RecordFn = RecordUpdateX86FP80;
  else if (Ty->isFP128Ty())
    RecordFn = RecordUpdateFP128;
  else if (Ty->isPPC_FP128Ty())
    RecordFn = RecordUpdatePPCFP128;
  else if (Ty->isVoidTy() || Ty->isLabelTy() || Ty->isMetadataTy())
    return nullptr;
  else
    return nullptr;

  // Arguments to pass to the recording function
  Value *Args[2] = {
    ConstantInt::get(Int32Ty, InstructionIndex, false), // Instruction index
    (CastInstr ? CastInstr : &I) // Value (after casting, if any)
  };

  CallInst *RecordCall = CallInst::Create(RecordFn, Args);
  assert(RecordCall && "Couldn't create call instruction.");

  if (CastInstr) {
    if (Before)
      CastInstr->insertBefore(Before);
    else
      CastInstr->insertAfter(&I);
    RecordCall->insertAfter(CastInstr);
  }
  else {
    if (Before)
      RecordCall->insertBefore(Before);
    else
      RecordCall->insertAfter(&I);
  }

  return RecordCall;
}

/// Perform module-level initialization before the pass is run.  For this
/// pass, we need to create function prototypes for the execution tracing
/// functions that will be called.
/// \param M a reference to the LLVM module to modify.
/// \return true if this LLVM module has been modified.
bool InsertExternalRecording::doInitialization(Module &M) {
  // Context of the instrumented module
  LLVMContext &Context = M.getContext();

  Int32Ty = Type::getInt32Ty(Context);
  Int64Ty = Type::getInt64Ty(Context);
  Int8PtrTy = Type::getInt8PtrTy(Context);

  // Get DataLayout
  DL = getAnalysisIfAvailable<DataLayout>();
  if (!DL)
    return false;

  // Index the module (prior to adding any functions)
  ModIndex.reset(new seec::ModuleIndex(M));

  // Build a list of all the global variables used in the module
  std::vector<Constant *> Globals;
  for (auto GIt = M.global_begin(), GEnd = M.global_end(); GIt != GEnd; ++GIt) {
    Globals.push_back(ConstantExpr::getPointerCast(&*GIt, Int8PtrTy));
  }

  auto GlobalsArrayType = ArrayType::get(Int8PtrTy, Globals.size());

  if (auto Existing = M.getNamedGlobal("SeeCInfoGlobals")) {
    Existing->eraseFromParent();
  }

  new GlobalVariable(M, GlobalsArrayType, true, GlobalValue::ExternalLinkage,
                     ConstantArray::get(GlobalsArrayType, Globals),
                     StringRef("SeeCInfoGlobals"));

  // Add a constant with the size of the globals array
  if (auto Existing = M.getNamedGlobal("SeeCInfoGlobalsLength")) {
    Existing->eraseFromParent();
  }

  new GlobalVariable(M, Int64Ty, true, GlobalValue::ExternalLinkage,
                     ConstantInt::get(Int64Ty, Globals.size()),
                     StringRef("SeeCInfoGlobalsLength"));

  // Build a list of all the functions used in the module.
  std::vector<Constant *> Functions;
  for (auto &F: M) {
    if (F.isIntrinsic()) {
      continue;
    }
    
    Functions.push_back(ConstantExpr::getPointerCast(&F, Int8PtrTy));
  }

  auto FunctionsArrayType = ArrayType::get(Int8PtrTy, Functions.size());

  if (auto Existing = M.getNamedGlobal("SeeCInfoFunctions")) {
    Existing->eraseFromParent();
  }

  new GlobalVariable(M, FunctionsArrayType, true, GlobalValue::ExternalLinkage,
                     ConstantArray::get(FunctionsArrayType, Functions),
                     StringRef("SeeCInfoFunctions"));

  // Add a constant with the size of the function array
  if (auto Existing = M.getNamedGlobal("SeeCInfoFunctionsLength")) {
    Existing->eraseFromParent();
  }

  new GlobalVariable(M, Int64Ty, true, GlobalValue::ExternalLinkage,
                     ConstantInt::get(Int64Ty, Functions.size()),
                     StringRef("SeeCInfoFunctionsLength"));

  // Add the module's identifier as a global string
  Constant *IdentifierStrConst
    = ConstantDataArray::getString(Context, M.getModuleIdentifier());
  new GlobalVariable(M, IdentifierStrConst->getType(), true,
                     GlobalValue::ExternalLinkage, IdentifierStrConst,
                     StringRef("SeeCInfoModuleIdentifier"));

  // Add declarations for the SeeC recording functions
  #define HANDLE_RECORD_POINT(POINT, LLVM_FUNCTION_TYPE) \
  Record##POINT = cast<Function>( \
    M.getOrInsertFunction("SeeCRecord" #POINT, \
      TypeBuilder<LLVM_FUNCTION_TYPE, true>::get(Context)));
#include "seec/Transforms/RecordExternal/RecordPoints.def"

  // check for any functions which will be replaced by SeeC interceptor
  // functions.
  for (auto &F : M) {
    auto Name = F.getName();
    llvm::Function *Intercept = nullptr;
    
#define SEEC_STRINGIZE2(STR) #STR
#define SEEC_STRINGIZE(STR) SEEC_STRINGIZE2(STR)

#define SEEC_INTERCEPTED_FUNCTION(NAME)                                        \
    if (Name.equals(#NAME)) {                                                  \
      auto NewName = SEEC_STRINGIZE(SEEC_MANGLE_FUNCTION(NAME));               \
      Intercept = createFunctionInterceptorPrototype(&F, NewName);             \
    }
#include "seec/Runtimes/Tracer/InterceptedFunctions.def"

#undef SEEC_STRINGIZE
#undef SEEC_STRINGIZE2
    
    if (Intercept)
      FunctionInterceptions.insert(std::make_pair(&F, Intercept));
  }

  return true;
}

/// Instrument a single function.
/// \param F the function to instrument.
/// \return true if the function was modified.
bool InsertExternalRecording::runOnFunction(Function &F) {
  if (!DL)
    return false;

  // Get a list of all the instructions in the function, so that we can visit
  // them without considering any new instructions inserted during the process.
  for (auto It = inst_begin(F), End = inst_end(F); It != End; ++It)
    FunctionInstructions.push_back(&*It);

  auto InstrIt = FunctionInstructions.begin();
  auto InstrEnd = FunctionInstructions.end();

  // Visit each original instruction for instrumentation
  for (InstructionIndex = 0; InstrIt != InstrEnd; ++InstrIt) {
    visit(*InstrIt);
    ++InstructionIndex;
  }

  // Insert function begin call, after any alloca's. We do this after
  // instrumenting instructions, so that the function start notification can
  // occur after alloca's but before the first alloca notification, without
  // any special logic in the alloca instrumentation.

  // Find the first non-alloca instruction
  auto AllocaIt = inst_begin(F);
  for (auto End = inst_end(F);
       AllocaIt != End && isa<AllocaInst>(&*AllocaIt);
       ++AllocaIt) {} // Intentionally empty

  // Get a constant int for the index of this function
  auto FunctionIndex (ModIndex->getIndexOfFunction(&F));
  if (!FunctionIndex.assigned()) {
    return false;
  }

  Value *Args[] = {
    ConstantInt::get(Int32Ty, (uint32_t) FunctionIndex.get<0>(), false)
  };

  // Pass the index to the function begin notification
  CallInst *CI = CallInst::Create(RecordFunctionBegin, Args);
  assert(CI && "Couldn't create call instruction.");

  CI->insertBefore(&*AllocaIt);
  
  // If this is main(), insert notifications for the strings we can read (args,
  // env).
  if (F.getName().equals("main")) {
    // Record env, if it is used.
    if (F.arg_size() >= 3) {
      llvm::Value *ArgEnvPtr = &*++(++(F.arg_begin()));
      
      if (ArgEnvPtr->getType() != Int8PtrTy) {
        auto Cast = new BitCastInst(ArgEnvPtr, Int8PtrTy);
        Cast->insertBefore(&*AllocaIt);
        ArgEnvPtr = Cast;
      }
      
      Value *CallArgs[] = { ArgEnvPtr };
      CallInst *Call = CallInst::Create(RecordEnv, CallArgs);
      assert(Call && "Couldn't create call instruction.");
      
      Call->insertBefore(&*AllocaIt);
    }
    
    // Record argv, if it is used.
    if (F.arg_size() >= 2) {
      auto ArgIt = F.arg_begin();
      
      llvm::Value *ArgArgCPtr = &*ArgIt;
      llvm::Value *ArgArgVPtr = &*++ArgIt;
      
      // If argc is less than 64 bits, we must extend it to 64 bits (because
      // this is what the recording function expects).
      auto IntTy = dyn_cast<IntegerType>(ArgArgCPtr->getType());
      assert(IntTy && "First argument to main() is not an integer type.");
      
      if (IntTy->getBitWidth() < 64) {
        auto Cast = new SExtInst(ArgArgCPtr, Int64Ty);
        Cast->insertBefore(&*AllocaIt);
        ArgArgCPtr = Cast;
      }
      
      if (ArgArgVPtr->getType() != Int8PtrTy) {
        auto Cast = new BitCastInst(ArgArgVPtr, Int8PtrTy);
        Cast->insertBefore(&*AllocaIt);
        ArgArgVPtr = Cast;
      }
      
      Value *CallArgs[] = {ArgArgCPtr, ArgArgVPtr};
      CallInst *Call = CallInst::Create(RecordArgs, CallArgs);
      assert(Call && "Couldn't create call instruction.");
      
      Call->insertBefore(&*AllocaIt);
    }
  }

  // Clear FunctionInstructions so that it's ready for the next Function
  FunctionInstructions.clear();

  return true;
}

/// Determine whether or not this pass will invalidate any analyses.
void InsertExternalRecording::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

///
void InsertExternalRecording::visitBinaryOperator(BinaryOperator &I) {
  switch (I.getOpcode()) {
    case llvm::Instruction::BinaryOps::UDiv: // Fall-through intentional.
    case llvm::Instruction::BinaryOps::SDiv: // Fall-through intentional.
    case llvm::Instruction::BinaryOps::FDiv: // Fall-through intentional.
    case llvm::Instruction::BinaryOps::URem: // Fall-through intentional.
    case llvm::Instruction::BinaryOps::SRem: // Fall-through intentional.
    case llvm::Instruction::BinaryOps::FRem:
      {
        Value *Args[] = { ConstantInt::get(Int32Ty, InstructionIndex, false) };
        CallInst::Create(RecordPreDivide, Args, "", &I);
      }
    default:
      break;
  }
  
  insertRecordUpdateForValue(I);
}

/// Insert a call to a tracing function before a return instruction.
/// \param I a reference to the return instruction
void InsertExternalRecording::visitReturnInst(ReturnInst &I) {
  // Get a constant int for the index of this function
  auto FunctionIndex (ModIndex->getIndexOfFunction(I.getParent()->getParent()));
  if (!FunctionIndex.assigned()) {
    return;
  }

  Value *Args[] = {
    ConstantInt::get(Int32Ty, (uint32_t) FunctionIndex.get<0>(), false)
  };

  CallInst::Create(RecordFunctionEnd, Args, "", &I);
}

/// Insert a call to a tracing function after an alloca instruction.
/// \param I a reference to the alloca instruction
void InsertExternalRecording::visitAllocaInst(AllocaInst &I) {
  // Find the first original instruction after I which isn't an AllocaInst
  Instruction *FirstNonAlloca = nullptr;

  auto InstructionCount = FunctionInstructions.size();
  for (auto i = InstructionIndex + 1; i < InstructionCount; ++i) {
    if (!isa<AllocaInst>(FunctionInstructions[i])) {
      FirstNonAlloca = FunctionInstructions[i];
      break;
    }
  }

  assert(FirstNonAlloca && "Couldn't find non-alloca Instruction");

  insertRecordUpdateForValue(I, FirstNonAlloca);
}

/// Insert a call to a tracing function prior to a load instruction.
/// \param LI a reference to the load instruction.
void InsertExternalRecording::visitLoadInst(LoadInst &LI) {
  // Create an array with the arguments
  Value *Args[] = {
    // The index of LI in this function's instruction list
    ConstantInt::get(Int32Ty, InstructionIndex, false),

    // The load pointer, cast to an i8*
    CastInst::CreatePointerCast(LI.getPointerOperand(), Int8PtrTy, "", &LI),

    // The size of the store, as an i64
    ConstantInt::get(Int64Ty, DL->getTypeStoreSize(LI.getType()), false)
  };

  // Create the call to the recording function, prior to the load instruction
  CallInst::Create(RecordPreLoad, Args, "", &LI);

  // Call update function for the load's value
  insertRecordUpdateForValue(LI);

  // Create post-load call, which will be inserted prior to the value update
  CallInst *PostCall = CallInst::Create(RecordPostLoad, Args);
  assert(PostCall && "Couldn't create call instruction.");
  PostCall->insertAfter(&LI);
}

/// Insert calls to tracing functions prior to, and following, a store
/// instruction.
/// \param SI a reference to the store instruction.
void InsertExternalRecording::visitStoreInst(StoreInst &SI) {
  Value *StoreValue = SI.getValueOperand();

  // Create an array with the arguments
  Value *Args[] = {
    // The index of SI in this Function
    ConstantInt::get(Int32Ty, InstructionIndex),

    // The store pointer, cast to an i8*
    CastInst::CreatePointerCast(SI.getPointerOperand(), Int8PtrTy, "", &SI),

    // The size of the store, as an i64
    ConstantInt::get(Int64Ty, DL->getTypeStoreSize(StoreValue->getType()))
  };

  // Create the call to the recording function prior to the store
  CallInst::Create(RecordPreStore, Args, "", &SI);

  // Create the call to the recording function following the store
  CallInst *PostCall = CallInst::Create(RecordPostStore, Args);
  assert(PostCall && "Couldn't create call instruction.");
  PostCall->insertAfter(&SI);
}

/// Insert calls to tracing functions to handle a call instruction. There are
/// three tracing functions: pre-call, post-call, and the generic update for
/// the return value, if it is valid.
/// \param I a reference to the instruction.
void InsertExternalRecording::visitCallInst(CallInst &CI) {
  Function *CalledFunction = CI.getCalledFunction();
  
  // Rewrite this call as a call to SeeC's interception function.
  if (FunctionInterceptions.count(CalledFunction)) {
    
  }

  // Get the called Value or Function
  Value *CalledValue;

  if (CalledFunction) {
    StringRef Name = CalledFunction->getName();

    // Don't instrument certain functions
#define FUNCTION_NOT_INSTRUMENTED(NAME) \
    if (Name == #NAME) \
      return;
#define FUNCTION_GROUP_NOT_INSTRUMENTED(PREFIX) \
    if (Name.startswith(#PREFIX)) \
      return;
#include "seec/Transforms/FunctionsNotInstrumented.def"

    CalledValue = CalledFunction;
  }
  else {
    CalledValue = CI.getCalledValue();
  }

  auto IndexConstant = ConstantInt::get(Int32Ty, InstructionIndex);

  // If the function is intrinsic, we can't pass a called address.
  if (CalledFunction && CalledFunction->isIntrinsic()) {
    Value *Args[] = { IndexConstant };

    // Call pre-call function
    CallInst::Create(RecordPreCallIntrinsic, Args, "", &CI);

    // Call post-call function
    CallInst *PostCall = CallInst::Create(RecordPostCallIntrinsic, Args);
    assert(PostCall && "Couldn't create call instruction.");
    PostCall->insertAfter(&CI);
  }
  else {
    Value *Args[] = {
      IndexConstant,
      new BitCastInst(CalledValue, Int8PtrTy, "", &CI) // Call address
    };

    // Call pre-call function
    CallInst::Create(RecordPreCall, Args, "", &CI);

    // Call post-call function
    CallInst *PostCall = CallInst::Create(RecordPostCall, Args);
    assert(PostCall && "Couldn't create call instruction.");
    PostCall->insertAfter(&CI);
  }

  // Call update function, if we have one for this type
  // Insert here, so that the update will be called BEFORE the post-call
  insertRecordUpdateForValue(CI);
}

} // namespace llvm
