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

#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <vector>
#include <cassert>

namespace llvm {

static bool IsMangledInterceptor(Function &F)
{
  return F.getName().startswith("__SeeC_")
      && F.getName().endswith("__");
}

static llvm::Function *GetInterceptorFor(Function &F, Module &M)
{
  llvm::SmallString<128> InterceptorName;
  InterceptorName += "__SeeC_";
  InterceptorName += F.getName();
  InterceptorName += "__";
  
  return M.getFunction(InterceptorName);
}

char InsertExternalRecording::ID = 0;

llvm::Function *
InsertExternalRecording::
createFunctionInterceptorPrototype(llvm::Function *ForFn,
                                   llvm::StringRef NewName)
{
  auto Mod = ForFn->getParent();
  if (auto ExistingFn = Mod->getFunction(NewName)) {
    // TODO: Check type.
    return ExistingFn;
  }
  
  auto NewFn = Mod->getOrInsertFunction(NewName,
                                        ForFn->getFunctionType(),
                                        ForFn->getAttributes());
  
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

/// \brief Get the bitcode for M as a string.
///
static std::string GetModuleBitcode(Module &M) {
  std::string ModuleBitcode;
  
  llvm::raw_string_ostream BitcodeStream {ModuleBitcode};
  llvm::WriteBitcodeToFile(&M, BitcodeStream);
  BitcodeStream.flush();
  
  return ModuleBitcode;
}

/// \brief Get a vector of Constant pointers to the globals in M.
///
static std::vector<Constant *> GetGlobals(Module &M, llvm::Type *Int8PtrTy) {
  std::vector<Constant *> Globals;
  
  for (auto GIt = M.global_begin(), GEnd = M.global_end(); GIt != GEnd; ++GIt) {
    Globals.push_back(ConstantExpr::getPointerCast(&*GIt, Int8PtrTy));
  }
  
  return Globals;
}

/// \brief Get a vector of Constant pointers to the functions in M.
///
static std::vector<Constant *> GetFunctions(Module &M, llvm::Type *Int8PtrTy) {
  std::vector<Constant *> Functions;
  
  for (auto &F: M) {
    if (F.isIntrinsic()) {
      continue;
    }
    
    Functions.push_back(ConstantExpr::getPointerCast(&F, Int8PtrTy));
  }
  
  return Functions;
}

/// \brief Add a lookup array to M.
///
static void AddLookupArray(Module &M,
                           std::vector<Constant *> const &Contents,
                           StringRef LookupName,
                           StringRef LookupLengthName)
{
  auto &Context = M.getContext();
  auto const Int64Ty = Type::getInt64Ty(Context);
  auto const Int8PtrTy = Type::getInt8PtrTy(Context);
  auto const ArrayTy = ArrayType::get(Int8PtrTy, Contents.size());

  if (auto Existing = M.getNamedGlobal(LookupName)) {
    Existing->eraseFromParent();
  }

  new GlobalVariable(M, ArrayTy, true, GlobalValue::ExternalLinkage,
                     ConstantArray::get(ArrayTy, Contents),
                     LookupName);

  // Add a constant with the size of the array.
  if (auto Existing = M.getNamedGlobal(LookupLengthName)) {
    Existing->eraseFromParent();
  }

  new GlobalVariable(M, Int64Ty, true, GlobalValue::ExternalLinkage,
                     ConstantInt::get(Int64Ty, Contents.size()),
                     LookupLengthName);
}

/// \brief Add information about the Module M to itself.
///
static void AddModuleInfo(Module &M,
                          std::string const &ModuleBitcode)
{
  auto &Context = M.getContext();
  auto const Int64Ty = Type::getInt64Ty(Context);
  
  // Add the module's bitcode as a global.
  if (auto Existing = M.getNamedGlobal("SeeCInfoModuleBitcode")) {
    Existing->eraseFromParent();
  }
  
  auto BitcodeConst = ConstantDataArray::getString(Context, ModuleBitcode);
  new GlobalVariable(M, BitcodeConst->getType(), true,
                     GlobalValue::ExternalLinkage, BitcodeConst,
                     StringRef("SeeCInfoModuleBitcode"));
  
  // Add the size of the module's bitcode as a global.
  if (auto Existing = M.getNamedGlobal("SeeCInfoModuleBitcodeLength")) {
    Existing->eraseFromParent();
  }
  
  new GlobalVariable(M, Int64Ty, true, GlobalVariable::ExternalLinkage,
                     ConstantInt::get(Int64Ty, ModuleBitcode.size()),
                     StringRef("SeeCInfoModuleBitcodeLength"));

  // Add the module's identifier as a global string
  Constant *IdentifierStrConst
    = ConstantDataArray::getString(Context, M.getModuleIdentifier());
  new GlobalVariable(M, IdentifierStrConst->getType(), true,
                     GlobalValue::ExternalLinkage, IdentifierStrConst,
                     StringRef("SeeCInfoModuleIdentifier"));
}

///
///
static void ReplaceUsesWithInterceptor(Function *Original,
                                       Function *Interceptor)
{
  auto       It  = Original->use_begin();
  auto const End = Original->use_end();

  while (It != End) {
    auto Current = It++;
    auto TheUser = *Current;

    if (auto C = dyn_cast<Constant>(TheUser)) {
      if (!isa<GlobalValue>(C)) {
        C->replaceUsesOfWithOnConstant(Original, Interceptor,
                                       &Current.getUse());
      }
    }
    else if (auto I = dyn_cast<Instruction>(TheUser)) {
      if (I->getParent()->getParent() != Interceptor) {
        Current.getUse().set(Interceptor);
      }
    }
  }
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
  
  // Get bitcode for the uninstrumented Module.
  std::string const ModuleBitcode = GetModuleBitcode(M);

  AddLookupArray(M, GetGlobals(M, Int8PtrTy),
                 "SeeCInfoGlobals", "SeeCInfoGlobalsLength");
  AddLookupArray(M, GetFunctions(M, Int8PtrTy),
                 "SeeCInfoFunctions", "SeeCInfoFunctionsLength");
  AddModuleInfo(M, ModuleBitcode);

  // Add the path to the SeeC installation.
  if (auto Existing = M.getNamedGlobal("__SeeC_ResourcePath__"))
    Existing->eraseFromParent();

  auto const PathConst = llvm::ConstantDataArray::getString(Context,
                                                            ResourcePath);
  new llvm::GlobalVariable(M, PathConst->getType(), true,
                           llvm::GlobalValue::ExternalLinkage, PathConst,
                           llvm::StringRef("__SeeC_ResourcePath__"));

  // Check for unhandled external functions.
  for (auto &F : M) {
    if (F.empty() && !F.isIntrinsic()) {
      auto Name = F.getName();
      
      // Don't consider the "\01_" prefix when matching names.
      if (Name.startswith("\01_"))
        Name = Name.substr(std::strlen("\01_"));
      
      bool Handled = false;
      
#define SEEC_FUNCTION_HANDLED(NAME) if (Name.equals(#NAME)) Handled = true;
#include "seec/Transforms/FunctionsHandled.def"

#define HANDLE_RECORD_POINT(POINT, LLVM_FUNCTION_TYPE) \
      if (Name.equals("SeeCRecord" #POINT)) Handled = true;
#include "seec/Transforms/RecordExternal/RecordPoints.def"

      if (Handled || IsMangledInterceptor(F) || GetInterceptorFor(F, M))
        continue;
      
      UnhandledFunctions.insert(&F);
    }
  }

  // Add declarations for the SeeC recording functions
  #define HANDLE_RECORD_POINT(POINT, LLVM_FUNCTION_TYPE) \
  Record##POINT = cast<Function>( \
    M.getOrInsertFunction("SeeCRecord" #POINT, \
      TypeBuilder<LLVM_FUNCTION_TYPE, true>::get(Context)));
#include "seec/Transforms/RecordExternal/RecordPoints.def"

  // Perform SeeC's function interception.
  for (auto &F : M) {
    if (!F.empty())
      continue;
    
    auto Name = F.getName();
    
    // Don't consider the "\01_" prefix when matching names.
    if (Name.startswith("\01_"))
      Name = Name.substr(std::strlen("\01_"));
    
    llvm::Function *Intercept = nullptr;
    
#define SEEC__STRINGIZE2(STR) #STR
#define SEEC__STRINGIZE(STR) SEEC__STRINGIZE2(STR)

#define SEEC_INTERCEPTED_FUNCTION(NAME)                                        \
    if (Name.equals(#NAME)) {                                                  \
      auto NewName = SEEC__STRINGIZE(SEEC_MANGLE_FUNCTION(NAME));              \
      Intercept = createFunctionInterceptorPrototype(&F, NewName);             \
    }

#define SEEC_INTERCEPTED_FUNCTION_ALIAS(ALIAS, NAME)                           \
    if (Name.equals(#ALIAS)) {                                                 \
      auto NewName = SEEC__STRINGIZE(SEEC_MANGLE_FUNCTION(NAME));              \
      Intercept = createFunctionInterceptorPrototype(&F, NewName);             \
    }

#include "seec/Runtimes/Tracer/InterceptedFunctions.def"

#undef SEEC__STRINGIZE
#undef SEEC__STRINGIZE2

    if (!Intercept)
      Intercept = GetInterceptorFor(F, M);

    if (Intercept) {
      ReplaceUsesWithInterceptor(&F, Intercept);
      Interceptors.insert(std::make_pair(&F, Intercept));
    }
  }
  
  return true;
}

bool InsertExternalRecording::runOnFunction(Function &F) {
  if (!DL)
    return false;

  // If the function is an interceptor, or has an interceptor available, then
  // we should not instrument it.
  if (IsMangledInterceptor(F) || Interceptors.find(&F) != Interceptors.end())
    return false;

  // Get a list of all the instructions in the function, so that we can visit
  // them without considering any new instructions inserted during the process.
  for (auto It = inst_begin(F), End = inst_end(F); It != End; ++It)
    FunctionInstructions.push_back(&*It);

  // Insert function entry notifications.
  auto const FirstIn = FunctionInstructions.front();

  // Get a constant int for the index of this function
  auto const FunctionIndex (ModIndex->getIndexOfFunction(&F));
  if (!FunctionIndex.assigned())
    return false;

  Value *Args[] = {
    ConstantInt::get(Int32Ty, (uint32_t) FunctionIndex.get<0>(), false)
  };

  // Pass the index to the function begin notification
  CallInst *CI = CallInst::Create(RecordFunctionBegin, Args, "", FirstIn);
  assert(CI && "Couldn't create call instruction.");

  if (!F.getName().equals("main")) {
    // F is not main(): insert notifications for all argument values.
    uint32_t ArgIndex = 0;
    
    for (auto &Arg : seec::range(F.arg_begin(), F.arg_end())) {
      if (Arg.hasByValAttr()) {
        llvm::Value *ArgPtr = &Arg;
        
        if (ArgPtr->getType() != Int8PtrTy) {
          auto Cast = new BitCastInst(ArgPtr, Int8PtrTy, "", FirstIn);
          ArgPtr = Cast;
        }
        
        Value * const CallArgs[] = {
          ConstantInt::get(Int32Ty, ArgIndex),
          ArgPtr
        };
        
        auto Call = CallInst::Create(RecordArgumentByVal, CallArgs, "",FirstIn);
        assert(Call && "Couldn't create call instruction.");
      }
      
      ++ArgIndex;
    }
  }
  else {
    // F is main(): insert notifications for the strings we can read.
    
    // Record env, if it is used.
    if (F.arg_size() >= 3) {
      llvm::Value *ArgEnvPtr = &*++(++(F.arg_begin()));
      
      if (ArgEnvPtr->getType() != Int8PtrTy) {
        auto Cast = new BitCastInst(ArgEnvPtr, Int8PtrTy, "", FirstIn);
        ArgEnvPtr = Cast;
      }
      
      Value *CallArgs[] = { ArgEnvPtr };
      CallInst *Call = CallInst::Create(RecordEnv, CallArgs, "", FirstIn);
      assert(Call && "Couldn't create call instruction.");
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
        auto Cast = new SExtInst(ArgArgCPtr, Int64Ty, "", FirstIn);
        ArgArgCPtr = Cast;
      }
      
      if (ArgArgVPtr->getType() != Int8PtrTy) {
        auto Cast = new BitCastInst(ArgArgVPtr, Int8PtrTy, "", FirstIn);
        ArgArgVPtr = Cast;
      }
      
      Value *CallArgs[] = {ArgArgCPtr, ArgArgVPtr};
      CallInst *Call = CallInst::Create(RecordArgs, CallArgs, "", FirstIn);
      assert(Call && "Couldn't create call instruction.");
    }
  }

  // Visit each original instruction for instrumentation
  InstructionIndex = 0;
  for (auto const Instr : FunctionInstructions) {
    visit(Instr);
    ++InstructionIndex;
  }

  // Clear FunctionInstructions so that it's ready for the next Function
  FunctionInstructions.clear();

  return true;
}

void InsertExternalRecording::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

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
    ConstantInt::get(Int32Ty, (uint32_t) FunctionIndex.get<0>(), false),
    ConstantInt::get(Int32Ty, InstructionIndex, false),
  };

  CallInst::Create(RecordFunctionEnd, Args, "", &I);
}

static llvm::Value *getAsInt64Ty(llvm::Value * const V,
                                 llvm::Type * const Int64Ty,
                                 llvm::Instruction * const Before)
{
  if (V->getType()->isIntegerTy(64))
    return V;

  if (auto const CV = llvm::dyn_cast<llvm::ConstantInt>(V))
    return ConstantInt::get(Int64Ty, CV->getZExtValue());
  else
    return new ZExtInst(V, Int64Ty, "", Before);
}

/// Insert a call to a tracing function after an alloca instruction.
/// \param I a reference to the alloca instruction
void InsertExternalRecording::visitAllocaInst(AllocaInst &I) {
  Value *Args[] = {
    ConstantInt::get(Int32Ty, InstructionIndex, false),
    ConstantInt::get(Int64Ty, DL->getTypeAllocSize(I.getAllocatedType())),
    getAsInt64Ty(I.getArraySize(), Int64Ty, &I)
  };

  CallInst::Create(RecordPreAlloca, Args, "", &I);

  insertRecordUpdateForValue(I);
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
  
  // Check if the call should be redirected to an interception function (either
  // a user-defined one, or one provided by SeeC). If it is redirected then we
  // only need to notify the instruction index before the call - checking,
  // recording and value updating must be performed by the interceptor.
  bool IsIntercepted = false;

  if (CalledFunction && IsMangledInterceptor(*CalledFunction))
    IsIntercepted = true;
  else {
    auto const InterceptorIt = Interceptors.find(CalledFunction);
    if (InterceptorIt != Interceptors.end()) {
      CI.setCalledFunction(InterceptorIt->second);
      IsIntercepted = true;
    }
  }

  if (IsIntercepted) {
    Value *Args[] = {ConstantInt::get(Int32Ty, InstructionIndex)};
    CallInst::Create(RecordSetInstruction, Args, "", &CI);
    return;
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
