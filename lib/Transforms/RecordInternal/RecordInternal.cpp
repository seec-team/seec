//===- RecordInternal.cpp - Insert execution tracing ---------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "seec"

#include "seec/Transforms/RecordInternal/RecordInternal.hpp"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Type.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TypeBuilder.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <vector>
#include <cassert>

using namespace llvm;

namespace seec {

// class InternalRecordingListener

/// Find an Instruction in the current Function by its index.
/// \param InstructionIndex the index of the Instruction (0-based).
/// \return The Instruction at position InstructionIndex in the currently
///         executing function.
Instruction *
InternalRecordingListener::getInstruction(uint32_t InstructionIndex) {
  if (CallStack.size() == 0)
    return nullptr;

  auto CurrentFunction = CallStack.back();

  auto FunctionIndex = OriginalModuleIndex.getFunctionIndex(CurrentFunction);
  if (!FunctionIndex)
    return nullptr;

  return FunctionIndex->getInstruction(InstructionIndex);
}

/// Receive notification that a new instrumented Function has been entered.
/// \param F the Function which has been entered.
void InternalRecordingListener::recordFunctionBegin(Function *F) {
  Listener->notifyFunctionBegin(F);
  CallStack.push_back(F);
}

/// Receive notification that the current instrumented Function is being left.
void InternalRecordingListener::recordFunctionEnd() {
  Listener->notifyFunctionEnd(CallStack.back());
  CallStack.pop_back();
}

/// Receive notification that a CallInst is next to execute.
/// \param InstructionIndex index of the CallInst.
void InternalRecordingListener::recordPreCall(uint32_t IIndex, void *Address) {
  Instruction *I = getInstruction(IIndex);
  assert(I && "InstructionIndex does not map to an Instruction!");

  CallInst *CI = dyn_cast<CallInst>(I);
  assert(CI && "InstructionIndex does not map to a CallInst!");

  Listener->notifyPreCall(IIndex, CI, Address);
}

/// Receive notification that a CallInst has finished executing.
/// \param InstructionIndex index of the CallInst.
void InternalRecordingListener::recordPostCall(uint32_t IIndex, void *Address) {
  Instruction *I = getInstruction(IIndex);
  assert(I && "InstructionIndex does not map to an Instruction!");

  CallInst *CI = dyn_cast<CallInst>(I);
  assert(CI && "InstructionIndex does not map to a CallInst!");

  Listener->notifyPostCall(IIndex, CI, Address);
}

/// Receive notification that an intrinsic CallInst is next to execute.
/// \param InstructionIndex index of the CallInst.
void InternalRecordingListener::recordPreCallIntrinsic(uint32_t IIndex) {
  Instruction *I = getInstruction(IIndex);
  assert(I && "InstructionIndex does not map to an Instruction!");

  CallInst *CI = dyn_cast<CallInst>(I);
  assert(CI && "InstructionIndex does not map to a CallInst!");

  Listener->notifyPreCallIntrinsic(IIndex, CI);
}

/// Receive notification that an intrinsic CallInst has finished executing.
/// \param InstructionIndex index of the CallInst.
void InternalRecordingListener::recordPostCallIntrinsic(uint32_t IIndex) {
  Instruction *I = getInstruction(IIndex);
  assert(I && "InstructionIndex does not map to an Instruction!");

  CallInst *CI = dyn_cast<CallInst>(I);
  assert(CI && "InstructionIndex does not map to a CallInst!");

  Listener->notifyPostCallIntrinsic(IIndex, CI);
}

/// Receive notification that a LoadInst is next to execute.
/// \param InstructionIndex index of the LoadInst.
/// \param Address address that will be loaded from.
/// \param Length number of bytes that will be loaded.
void InternalRecordingListener::recordLoad(uint32_t InstructionIndex,
                                           void *Address, uint64_t Length) {
  LoadInst *I = dyn_cast_or_null<LoadInst>(getInstruction(InstructionIndex));
  assert(I && "InstructionIndex does not map correctly!");

  Listener->notifyPreLoad(InstructionIndex, I, Address, Length);
}

/// Receive notification that a StoreInst is next to execute.
/// \param InstructionIndex index of the StoreInst.
/// \param Address address that will be stored to.
/// \param Length number of bytes that will be stored.
void InternalRecordingListener::recordPreStore(uint32_t InstructionIndex,
                                               void *Address, uint64_t Length) {
  StoreInst *I = dyn_cast_or_null<StoreInst>(getInstruction(InstructionIndex));
  assert(I && "InstructionIndex does not map correctly!");

  Listener->notifyPreStore(InstructionIndex, I, Address, Length);
}

/// Receive notification that a StoreInst has finished executing.
/// \param InstructionIndex index of the StoreInst.
/// \param Address address that was stored to.
/// \param Length number of bytes that were stored.
void InternalRecordingListener::recordPostStore(uint32_t InstructionIndex,
                                                void *Address,
                                                uint64_t Length) {
  StoreInst *I = dyn_cast_or_null<StoreInst>(getInstruction(InstructionIndex));
  assert(I && "InstructionIndex does not map correctly!");

  Listener->notifyPostStore(InstructionIndex, I, Address, Length);
}

/// Receive notification that a pointer value has been updated.
/// \param InstructionIndex index of the Instruction for this value.
/// \param Value new value of the pointer.
void InternalRecordingListener::recordUpdatePointer(uint32_t InstructionIndex,
                                                    void *Value) {
  Instruction *I = getInstruction(InstructionIndex);
  assert(I && "InstructionIndex does not map correctly!");

  GenericValue GV(Value);

  Listener->notifyValue(InstructionIndex, I, GV);
}

/// Receive a new value for an integer with 64 or less bits.
/// \param InstructionIndex index of the Instruction whose value is being
///        updated.
/// \param Value the updated value for the Instruction, zero-extended if
///        necessary.
void InternalRecordingListener::recordUpdateInt64(uint32_t InstructionIndex,
                                                  uint64_t Value) {
  Instruction *I = getInstruction(InstructionIndex);
  assert(I && "InstructionIndex does not map correctly!");

  IntegerType *Ty = dyn_cast_or_null<IntegerType>(I->getType());
  assert(Ty && "Instruction does not have an Integer Type!");

  GenericValue GV;
  GV.IntVal = APInt(Ty->getBitWidth(), Value);

  Listener->notifyValue(InstructionIndex, I, GV);
}

/// Receive notification that a float value has been updated.
/// \param InstructionIndex index of the Instruction for this value.
/// \param Value the new value of the float.
void InternalRecordingListener::recordUpdateFloat(uint32_t InstructionIndex,
                                                  float Value) {
  Instruction *I = getInstruction(InstructionIndex);
  assert(I && "InstructionIndex does not map correctly!");

  GenericValue GV;
  GV.FloatVal = Value;

  Listener->notifyValue(InstructionIndex, I, GV);
}

/// Receive notification that a double value has been updated.
/// \param InstructionIndex index of the Instruction for this value.
/// \param Value the new value of the double.
void InternalRecordingListener::recordUpdateDouble(uint32_t InstructionIndex,
                                                   double Value) {
  Instruction *I = getInstruction(InstructionIndex);
  assert(I && "InstructionIndex does not map correctly!");

  GenericValue GV;
  GV.DoubleVal = Value;

  Listener->notifyValue(InstructionIndex, I, GV);
}

void InternalRecordingListener::redirect_exit(int Code) {
  ExitCode = Code;
  longjmp(ExitJump, 0);
}

int InternalRecordingListener::redirect_atexit(void (*function)(void)) {
  return 0;
}

// helper functions

/// Create a new FunctionType with arguments preprended to an existing type.
/// \param Ty the existing FunctionType.
/// \param Args the types to prepend to Ty's argument types.
/// \return the new FunctionType.
FunctionType *prependArgumentsToFunctionType(FunctionType *Ty,
                                             ArrayRef<Type *> Args) {
  SmallVector<Type *, 8> Params(Args.begin(), Args.end());

  Params.insert(Params.end(), Ty->param_begin(), Ty->param_end());

  FunctionType *NewTy = FunctionType::get(Ty->getReturnType(),
                                          Params,
                                          Ty->isVarArg());

  return NewTy;
}

/// Redirect all uses of a Function to use a stub which calls SeeC's replacement
/// function and passes a pointer to the given Listener.
/// \param M the Module containing the Function.
/// \param F the Function to redirect.
/// \param RedirectName the name of SeeC's replacement function.
/// \param StubName the name for the new stub Function.
/// \param ListenerAddress the address of the Listener.
/// \return the new stub Function.
Function *redirectFunction(Module &M, Function &F, StringRef RedirectName,
                           StringRef StubName, ConstantInt *ListenerAddress) {
  LLVMContext &Context = M.getContext();

  Function *RedirectStub
    = cast<Function>(M.getOrInsertFunction(StubName, F.getFunctionType()));

  assert(!RedirectStub->size() && "RedirectStub already exists?");

  // Create the body of the redirect stub
  Type *VoidPtrTy = Type::getInt8PtrTy(Context);

  BasicBlock *EntryBlock = BasicBlock::Create(Context, "", RedirectStub);

  IntToPtrInst *ListenerAddressPtr = new IntToPtrInst(ListenerAddress,
                                                      VoidPtrTy, "",
                                                      EntryBlock);

  // Create forwarding call inst
  FunctionType *NewTy = prependArgumentsToFunctionType(F.getFunctionType(),
                                                       VoidPtrTy);

  Function *Redirect = cast<Function>(M.getOrInsertFunction(RedirectName,
                                                            NewTy));

  SmallVector<Value *, 8> Args;
  Args.push_back(ListenerAddressPtr);
  for (auto It = RedirectStub->arg_begin(), End = RedirectStub->arg_end();
       It != End; ++It) {
    Args.push_back(&*It);
  }

  CallInst::Create(Redirect, Args, "", EntryBlock);

  // Create void return
  ReturnInst::Create(Context, 0, EntryBlock);

  // Replace uses of existing function with the redirect stub
  F.replaceAllUsesWith(RedirectStub);

  return RedirectStub;
}

}

namespace llvm {

// class TargetData

char InsertInternalRecording::ID = 0;

/// Insert a call to notify SeeC of the new run-time value of I.
/// \param I the Instruction whose new run-time value is being recorded.
/// \return The Instruction which calls the notification function.

CallInst *
InsertInternalRecording::insertRecordUpdateForValue(Instruction &I,
                                                    Instruction *Before) {
  LLVMContext &Context = I.getContext();
  Type const *Ty = I.getType();

  Instruction *InsertPoint = new IntToPtrInst(ListenerAddress,
                                              Type::getInt8PtrTy(Context));
  if (Before)
    InsertPoint->insertBefore(Before);
  else
    InsertPoint->insertAfter(&I);

  Function *RecordFn = nullptr;
  Value *Args[3] = {InsertPoint, nullptr, nullptr};

  // Get the recording function
  if (IntegerType const *IntTy = dyn_cast<IntegerType>(Ty)) {
    uint32_t BitWidth = IntTy->getBitWidth();

#define HANDLE_WIDTH_BELOW_OR_EQUAL(WIDTH) \
    if (BitWidth <= WIDTH) { \
      RecordFn = RecordUpdateInt ## WIDTH; \
      if (BitWidth != WIDTH) { \
        Instruction *ZExt \
          = new ZExtInst(&I, Type::getInt ## WIDTH ## Ty(Context)); \
        ZExt->insertAfter(InsertPoint); \
        Args[2] = InsertPoint = ZExt; \
      } \
    }

    HANDLE_WIDTH_BELOW_OR_EQUAL(8)
    else HANDLE_WIDTH_BELOW_OR_EQUAL(16)
    else HANDLE_WIDTH_BELOW_OR_EQUAL(32)
    else HANDLE_WIDTH_BELOW_OR_EQUAL(64)

#undef HANDLE_WIDTH_BELOW_OR_EQUAL

    if (!RecordFn) {
      errs() << "[RecordInternal] Can't handle i" << IntTy->getBitWidth()
             << "\n";
      return nullptr;
    }
  }
  else if (isa<PointerType>(Ty)) {
    RecordFn = RecordUpdatePointer;

    // If the pointer isn't an i8*, we need to bitcast it to one
    Type *VoidPtrTy = Type::getInt8PtrTy(Context);
    if (Ty != VoidPtrTy) {
      Instruction *BitCast = new BitCastInst(&I, VoidPtrTy);
      BitCast->insertAfter(InsertPoint);
      Args[2] = InsertPoint = BitCast;
    }
  }
  else if (Ty->isFloatTy())
    RecordFn = RecordUpdateFloat;
  else if (Ty->isDoubleTy())
    RecordFn = RecordUpdateDouble;
  else if (Ty->isVoidTy() || Ty->isLabelTy() || Ty->isMetadataTy())
    return nullptr;
  else {
    errs() << "[RecordInternal] Don't know how to record update to type: ";
    Ty->print(errs());
    errs() << "\n";

    return nullptr;
  }

  // Setup arguments
  if (!Args[1])
    Args[1] = ConstantInt::get(Int32Ty, InstructionIndex, false);
  if (!Args[2])
    Args[2] = &I;

  CallInst *RecordCall = CallInst::Create(RecordFn, Args);
  assert(RecordCall && "Couldn't create call instruction.");

  RecordCall->insertAfter(InsertPoint);

  return RecordCall;
}

/// Perform module-level initialization before the pass is run.  For this
/// pass, we need to create function prototypes for the execution tracing
/// functions that will be called.
/// \param M a reference to the LLVM module to modify.
/// \return true if this LLVM module has been modified.
bool InsertInternalRecording::doInitialization(Module &M) {
  // Get TargetData
  TD = getAnalysisIfAvailable<TargetData>();
  if (!TD) {
    errs() << "SeeC Recording needs TargetData!\n";
    return false;
  }

  // Context of the instrumented module
  LLVMContext &Context = M.getContext();

  // Create a constant integer with the address of our Listener, which will be
  // used by the instrumented program to call out to the Listener object.
  ListenerAddress = ConstantInt::get(Context,
                                     APInt(sizeof(uintptr_t) * CHAR_BIT,
                                           uintptr_t(Listener)));

#define HANDLE_RECORD_POINT(POINT, LLVM_FUNCTION_TYPE) \
  Record##POINT = cast<Function>( \
    M.getOrInsertFunction("SeeCRecord" #POINT, \
      TypeBuilder<LLVM_FUNCTION_TYPE, true>::get(Context)));
#include "seec/Transforms/RecordInternal/RecordPoints.def"

  FunctionIndex = 0;

  Int32Ty = Type::getInt32Ty(Context);

  for (Function &F: M) {
    StringRef Name = F.getName();

#define SEEC_REDIRECT_CALL(NAME, RET_TY, RET, PARAMETERS, ARGUMENTS) \
    if (Name == #NAME) { \
      seec::redirectFunction(M, F, "SeeCRedirect_" #NAME, \
                             "SeeCRedirectStub_" #NAME, ListenerAddress); \
    }
#include "seec/Transforms/RecordInternal/RedirectCalls.def"
  }

  return true;
}

/// Instrument a single function.
/// \param F the function to instrument.
/// \return true if the function was modified.
bool InsertInternalRecording::runOnFunction(Function &F) {
  if (!TD)
    return false;

  Function *OriginalFunction = OriginalModule->getFunction(F.getName());

  if (!OriginalFunction) // F is a redirect stub we created
    return false;

  LLVMContext &Context = F.getContext();

  // Get a list of all the instructions in the function, so that we can visit
  // them without considering any new instructions inserted during the process.
  typedef std::vector<Instruction*>::iterator InstIter;

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
    FunctionInstructions.push_back(&*I);

  InstructionIndex = 0;
  std::vector<Instruction*>::iterator I = FunctionInstructions.begin(),
                                      E = FunctionInstructions.end();

  // Visit each original instruction for instrumentation
  for (; I != E; ++I) {
    visit(*I);
    ++InstructionIndex;
  }

  // Insert function begin call, after any alloca's. We do this after
  // instrumenting instructions, so that the function start notification can
  // occur after alloca's but before the first alloca notification, without
  // any special logic in the alloca instrumentation.

  // Find the first non-alloca instruction
  for (I = FunctionInstructions.begin(); I != E && isa<AllocaInst>(*I); ++I)
    ; // Intentionally empty

  // Rewind to the last alloca instruction
  if (I != FunctionInstructions.begin())
    --I;

  // Pointer to the InternalRecording Listener
  IntToPtrInst *ListenerPointer =
    new IntToPtrInst(ListenerAddress, Type::getInt8PtrTy(Context));
  ListenerPointer->insertAfter(*I);

  // Pointer to the "original" copy of this Function
  IntToPtrInst *OriginalCopyPointer =
    new IntToPtrInst(ConstantInt::get(Context,
                                      APInt(sizeof(uintptr_t) * CHAR_BIT,
                                            uintptr_t(OriginalFunction))),
                     Type::getInt8PtrTy(Context));
  OriginalCopyPointer->insertAfter(ListenerPointer);

  Value *Args[2] = {
    ListenerPointer,
    OriginalCopyPointer
  };

  CallInst *CI = CallInst::Create(RecordFunctionBegin, Args);
  assert(CI && "Couldn't create call instruction.");
  CI->insertAfter(OriginalCopyPointer);

  FunctionInstructions.clear();
  ++FunctionIndex;

  return true;
}

/// Determine whether or not this pass will invalidate any analyses.

void InsertInternalRecording::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

/// Insert a call to a tracing function before a return instruction.
/// \param I a reference to the return instruction

void InsertInternalRecording::visitReturnInst(ReturnInst &I) {
  LLVMContext &Context = I.getContext();

  Value *Args[1] = {
    // Pointer to the InternalRecording Listener
    new IntToPtrInst(ListenerAddress, Type::getInt8PtrTy(Context), "", &I)
  };

  CallInst::Create(RecordFunctionEnd, Args, "", &I);
}

/// Insert a call to a tracing function after an alloca instruction.
/// \param I a reference to the alloca instruction

void InsertInternalRecording::visitAllocaInst(AllocaInst &I) {
  // Find the first original instruction after I which isn't an AllocaInst
  Instruction *FirstNonAlloca = 0;
  for (uint32_t i = InstructionIndex + 1; i < FunctionInstructions.size(); ++i)
    if (!isa<AllocaInst>(FunctionInstructions[i])) {
      FirstNonAlloca = FunctionInstructions[i];
      break;
    }
  assert(FirstNonAlloca && "Couldn't find non-alloca instruction!");

  insertRecordUpdateForValue(I, FirstNonAlloca);

  return;
}

/// Insert a call to a tracing function prior to a load instruction.
/// \param LI a reference to the load instruction.

void InsertInternalRecording::visitLoadInst(LoadInst &LI) {
  LLVMContext &Context = LI.getContext();

  // Create an array with the arguments
  Value *PreArgs[4] = {
    // Pointer to the InternalRecording Listener
    new IntToPtrInst(ListenerAddress, Type::getInt8PtrTy(Context), "", &LI),

    // The index of LI in this function's instruction list
    ConstantInt::get(Int32Ty, InstructionIndex, false),

    // The load pointer, cast to an i8*
    CastInst::CreatePointerCast(LI.getPointerOperand(),
                                Type::getInt8PtrTy(Context),
                                "",
                                &LI),

    // The size of the store, as an i64
    ConstantInt::get(Type::getInt64Ty(Context),
                     TD->getTypeStoreSize(LI.getType()),
                     false)
  };

  // Create the call to the recording function, prior to the load instruction
  CallInst::Create(RecordLoad, PreArgs, "", &LI);

  // Call update function, if we have one for this type
  insertRecordUpdateForValue(LI);

  return;
}

/// Insert calls to tracing functions prior to, and following, a store
/// instruction.
/// \param SI a reference to the store instruction.

void InsertInternalRecording::visitStoreInst(StoreInst &SI) {
  LLVMContext &Context = SI.getContext();

  StoreInst *Store = &SI;
  Value *StoreValue = SI.getValueOperand();

  // Create an array with the arguments
  Value *Args[4] = {
    // Pointer to the InternalRecording Listener
    new IntToPtrInst(ListenerAddress, Type::getInt8PtrTy(Context), "", Store),

    // The index of SI in this Function
    ConstantInt::get(Int32Ty, InstructionIndex, false),

    // The store pointer, cast to an i8*
    CastInst::CreatePointerCast(Store->getPointerOperand(),
                                Type::getInt8PtrTy(Context),
                                "",
                                Store),

    // The size of the store, as an i64
    ConstantInt::get(Type::getInt64Ty(Context),
                     TD->getTypeStoreSize(StoreValue->getType()),
                     false)
  };

  // Create the call to the recording function prior to the store
  CallInst::Create(RecordPreStore, Args, "", Store);

  // Create the call to the recording function following the store
  CallInst *PostCall = CallInst::Create(RecordPostStore, Args);
  assert(PostCall && "Couldn't create call instruction.");
  PostCall->insertAfter(Store);

  return;
}

/// Insert calls to tracing functions to handle a call instruction. There are
/// three tracing functions: pre-call, post-call, and the generic update for
/// the return value, if it is valid.
/// \param I a reference to the instruction.

void InsertInternalRecording::visitCallInst(CallInst &I) {
  Function *CalledFunction = I.getCalledFunction();

  Value *CalledValue;

  if (CalledFunction) {
    StringRef Name = CalledFunction->getName();

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
    CalledValue = I.getCalledValue();
  }

  LLVMContext &Context = I.getContext();

  Type *VoidPtrTy = Type::getInt8PtrTy(Context);

  IntToPtrInst *ListenerAddressPointer = new IntToPtrInst(ListenerAddress,
                                                          VoidPtrTy, "", &I);

  Constant *IndexConstant = ConstantInt::get(Int32Ty, InstructionIndex, false);

  CallInst *CI = &I;

  if (CalledFunction && CalledFunction->isIntrinsic()) {
    // Arguments passed to pre-call and post-call functions
    Value *Args[2] = { ListenerAddressPointer, IndexConstant };

    // Call pre-call function
    CallInst::Create(RecordPreCallIntrinsic, Args, "", CI);

    // Call post-call function
    CallInst *PostCall = CallInst::Create(RecordPostCallIntrinsic, Args);
    PostCall->insertAfter(CI);
  }
  else {
    BitCastInst *CallAddress = new BitCastInst(CalledValue, VoidPtrTy, "", CI);

    // Arguments passed to pre-call and post-call functions
    Value *Args[3] = { ListenerAddressPointer, IndexConstant, CallAddress };

    // Call pre-call function
    CallInst::Create(RecordPreCall, Args, "", CI);

    // Call post-call function
    CallInst *PostCall = CallInst::Create(RecordPostCall, Args);
    PostCall->insertAfter(CI);
  }

  // Call update function, if we have one for this type
  // Insert here, so that the update will be called BEFORE the post-call
  insertRecordUpdateForValue(*CI);
}

} // namespace llvm
