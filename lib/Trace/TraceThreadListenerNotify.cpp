//===- lib/Trace/TraceThreadListenerNotify.cpp ----------------------------===//
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

#include "seec/Trace/DetectCalls.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/CheckNew.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/IR/Argument.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

void TraceThreadListener::enterNotification() {
  synchronizeProcessTime();
  checkSignals();
}

void TraceThreadListener::exitNotification() {
  SupportSyncExit.getSynchronizedExit().check();
  
  if (EventsOut.offset() > ThreadEventLimit) {
    llvm::errs() << "\nSeeC: Thread event limit reached!\n";
    
    // Shut down the tracing.
    SupportSyncExit.getSynchronizedExit().exit(EXIT_SUCCESS);
  }
}

void TraceThreadListener::exitPreNotification() {
  exitNotification();
}

void TraceThreadListener::exitPostNotification() {
  if (GlobalMemoryLock)
    GlobalMemoryLock.unlock();
  
  if (DynamicMemoryLock)
    DynamicMemoryLock.unlock();
  
  if (StreamsLock)
    StreamsLock.unlock();
  
  if (DirsLock)
    DirsLock.unlock();
  
  clearCI();
  
  exitNotification();
}

void TraceThreadListener::notifyFunctionBegin(uint32_t Index,
                                              llvm::Function const *F) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  uint64_t Entered = ++Time;

  // Find the location that the new FunctionRecord will be placed at.
  auto RecordOffset = getNewFunctionRecordOffset();

  // Create function start event.
  auto StartOffset = EventsOut.write<EventType::FunctionStart>(RecordOffset);

  // Get the shared, indexed view of the function.
  auto const &FIndex = ProcessListener.moduleIndex().getFunctionIndex(Index);

  // Get object information for Arguments from the call site.
  llvm::DenseMap<llvm::Argument const *, uintptr_t> PtrArgObjects;

  if (ActiveFunction) {
    auto const Inst = ActiveFunction->getActiveInstruction();
    if (auto const Call = llvm::dyn_cast<llvm::CallInst>(Inst)) {
      // TODO: Ensure that the called Function is F.
      for (auto const &Arg : F->getArgumentList()) {
        if (Arg.getType()->isPointerTy()) {
          auto const Operand = Call->getArgOperand(Arg.getArgNo());
          auto const Object = ActiveFunction->getPointerObject(Operand);
          PtrArgObjects[&Arg] = Object;
        }
      }
    }
  }

  RecordedFunctions.emplace_back(
    seec::makeUnique<RecordedFunction>(RecordOffset,
                                       Index,
                                       StartOffset,
                                       Entered));

  // Add a TracedFunction to the stack and make it the ActiveFunction.
  {
    std::lock_guard<std::mutex> Lock(FunctionStackMutex);
    auto const Parent = ActiveFunction;

    FunctionStack.emplace_back(*this,
                               *FIndex,
                               *RecordedFunctions.back(),
                               std::move(PtrArgObjects));

    ActiveFunction = &(FunctionStack.back());

    // If there was already an active function, add the new function as a child,
    // otherwise record it as a new top-level function.
    if (Parent)
      Parent->addChild(*ActiveFunction);
    else
      RecordedTopLevelFunctions.emplace_back(RecordOffset);
  }
}

void TraceThreadListener::notifyArgumentByVal(uint32_t Index,
                                              llvm::Argument const *Arg,
                                              void const *Address) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  // Get general information about the argument.
  auto const AddressInt = reinterpret_cast<uintptr_t>(Address);
  
  auto const ArgType = Arg->getType();
  if (!ArgType->isPointerTy())
    return;
  
  auto const ArgPtrType = llvm::dyn_cast<llvm::PointerType>(ArgType);
  assert(ArgPtrType);
  
  auto const PointeeType = ArgPtrType->getPointerElementType();
  assert(PointeeType);
  
  auto const &DataLayout = ProcessListener.dataLayout();
  auto const PointeeSize = DataLayout.getTypeStoreSize(PointeeType);
  
  // Record this memory area in the trace.
  EventsOut.write<EventType::ByValRegionAdd>(Arg->getArgNo(),
                                             AddressInt,
                                             PointeeSize);
  
  // Lock global memory, and release when we exit scope.
  acquireGlobalMemoryWriteLock();
  auto UnlockGlobalMemory = scopeExit([=](){ GlobalMemoryLock.unlock(); });
  
  // Add the memory area of the argument.
  ActiveFunction->addByValArg(Arg, MemoryArea(AddressInt, PointeeSize));
  
  // We need to query the parent's FunctionRecord.
  TracedFunction const *ParentFunction = nullptr;
  
  {
    std::lock_guard<std::mutex> Lock(FunctionStackMutex);
    if (FunctionStack.size() >= 2) {
      auto ParentIdx = FunctionStack.size() - 2;
      ParentFunction = &(FunctionStack[ParentIdx]);
    }
  }
  
  // If we can find the original value, copy the memory from that.
  if (ParentFunction) {
    auto const ParentInstruction = ParentFunction->getActiveInstruction();
    if (ParentInstruction) {
      auto const ParentCall = llvm::dyn_cast<llvm::CallInst>(ParentInstruction);
      // TODO: handle indirect function calls.
      if (ParentCall->getCalledFunction() == Arg->getParent()) {
        auto const OrigOp = ParentCall->getOperand(Index);
        auto const OrigRTV
          = getCurrentRuntimeValueAs<uintptr_t>(*ParentFunction, OrigOp);
        assert(OrigRTV.assigned() && "Couldn't get pointer.");
        
        recordMemmove(OrigRTV.get<0>(), AddressInt, PointeeSize);
        return;
      }
    }
  }
  
  // Assume that the argument is initialized.
  recordUntypedState(reinterpret_cast<char const *>(Address), PointeeSize);
}

void TraceThreadListener::notifyArgs(uint64_t ArgC, char **ArgV) {
  // Handle common behaviour when entering and exiting notifications.
  // Note thatnotifyArgs has the exit behaviour of a post-notification, because
  // it effectively ends the FunctionStart block for main().
  enterNotification();
  auto OnExit = scopeExit([=](){exitPostNotification();});

  // Set the object of the argv Argument.
  auto const ArgVArg = ActiveFunction->getFunctionIndex().getArgument(1);
  ActiveFunction->setPointerObject(ArgVArg, reinterpret_cast<uintptr_t>(ArgV));
  
  GlobalMemoryLock = ProcessListener.lockMemory();
  
  // Make the pointer array read/write.
  auto TableAddress = reinterpret_cast<uintptr_t>(ArgV);
  auto TableSize = sizeof(char *[ArgC + 1]);
  
  addKnownMemoryRegion(TableAddress, TableSize, MemoryPermission::ReadWrite);
  
  // Set the state of the pointer array.
  recordUntypedState(reinterpret_cast<char const *>(ArgV), TableSize);
  
  // Now each of the individual strings.
  for (uint64_t i = 0; i < ArgC; ++i) {
    // Make the string read/write.
    auto StringAddress = reinterpret_cast<uintptr_t>(ArgV[i]);
    auto StringSize = strlen(ArgV[i]) + 1;
    
    addKnownMemoryRegion(StringAddress,
                         StringSize,
                         MemoryPermission::ReadWrite);
    
    // Set the state of the string.
    recordUntypedState(ArgV[i], StringSize);

    // Set the destination object of the pointer.
    auto const PtrLocation = reinterpret_cast<uintptr_t>(ArgV + i);
    ProcessListener.setInMemoryPointerObject(PtrLocation, StringAddress);
  }

  GlobalMemoryLock.unlock();
}

void TraceThreadListener::notifyEnv(char **EnvP) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});
  
  // Set the object of the envp Argument.
  auto const EnvPArg = ActiveFunction->getFunctionIndex().getArgument(2);
  ActiveFunction->setPointerObject(EnvPArg, reinterpret_cast<uintptr_t>(EnvP));

  GlobalMemoryLock = ProcessListener.lockMemory();
  
  // Find the number of pointers in the array (including the NULL pointer that
  // terminates the array).
  std::size_t Count = 0;
  while (EnvP[Count++]);
  
  // Make the pointer array readable.
  auto TableAddress = reinterpret_cast<uintptr_t>(EnvP);
  auto TableSize = sizeof(char *[Count]);
  
  addKnownMemoryRegion(TableAddress,
                       TableSize,
                       MemoryPermission::ReadOnly);
  
  // Set the state of the pointer array.
  recordUntypedState(reinterpret_cast<char const *>(EnvP), TableSize);
  
  // Now each of the individual strings. The limit is Count-1 because the final
  // entry in EnvP is a NULL pointer.
  for (std::size_t i = 0; i < Count - 1; ++i) {
    // Make the string readable.
    auto StringAddress = reinterpret_cast<uintptr_t>(EnvP[i]);
    auto StringSize = strlen(EnvP[i]) + 1;
    
    addKnownMemoryRegion(StringAddress,
                         StringSize,
                         MemoryPermission::ReadOnly);
    
    // Set the state of the string.
    recordUntypedState(EnvP[i], StringSize);
  }
  
  GlobalMemoryLock.unlock();
}

void TraceThreadListener::notifyFunctionEnd(uint32_t const Index,
                                            llvm::Function const *F,
                                            uint32_t const InstructionIndex,
                                            llvm::Instruction const *Terminator)
{
  // It's OK to check this without owning FunctionStackMutex, because the
  // FunctionStack can only be changed by a single thread.
  assert(!FunctionStack.empty() && "notifyFunctionEnd with empty stack.");

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  uint64_t Exited = ++Time;

  auto &Record = FunctionStack.back().getRecordedFunction();
  TracedFunction *ParentFunction = nullptr;

  if (FunctionStack.size() >= 2)
    ParentFunction = &FunctionStack[FunctionStack.size() - 2];

  // If the terminated Function returned a pointer, then transfer the correct
  // pointer object information to the parent Function's CallInst.
  if (ParentFunction && F->getType()->isPointerTy()) {
    if (auto const Ret = llvm::dyn_cast<llvm::ReturnInst>(Terminator)) {
      if (auto const RetVal = Ret->getReturnValue()) {
        auto const RetPtrObj = ActiveFunction->getPointerObject(RetVal);
        ParentFunction->setPointerObject(ParentFunction->getActiveInstruction(),
                                         RetPtrObj);
      }
    }
  }

  // Create function end event.
  auto EndOffset =
    EventsOut.write<EventType::FunctionEnd>(Record.getRecordOffset());

  // Clear stack allocations and pop the Function from the stack.
  {
    std::lock_guard<std::mutex> Lock(FunctionStackMutex);

    acquireGlobalMemoryWriteLock();
    auto UnlockGlobalMemory = scopeExit([=](){GlobalMemoryLock.unlock();});

    auto StackArea = ActiveFunction->getStackArea();
    recordStateClear(StackArea.address(), StackArea.length());

    for (auto const &Arg : ActiveFunction->getByValArgs()) {
      auto const &Area = Arg.getArea();
      recordStateClear(Area.address(), Area.length());
    }

    FunctionStack.pop_back();
    ActiveFunction = FunctionStack.empty() ? nullptr : &FunctionStack.back();
  }

  // Update the function record with the end details.
  Record.setCompletion(EndOffset, Exited);
}

void TraceThreadListener::notifyPreCall(uint32_t Index,
                                        llvm::CallInst const *CallInst,
                                        void const *Address) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPreNotification();});
  ActiveFunction->setActiveInstruction(CallInst);

  detectPreCall(CallInst, Index, Address);
  
  // Emit a PreInstruction so that the call becomes active.
  EventsOut.write<EventType::PreInstruction>(Index, ++Time);
}

void TraceThreadListener::notifyPostCall(uint32_t Index,
                                         llvm::CallInst const *CallInst,
                                         void const *Address) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPostNotification();});
  
  detectPostCall(CallInst, Index, Address);
}

void TraceThreadListener::notifyPreCallIntrinsic(uint32_t Index,
                                                 llvm::CallInst const *CI) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPreNotification();});
  ActiveFunction->setActiveInstruction(CI);

  auto Function = CI->getCalledFunction();
  auto ID = Function->getIntrinsicID();

  detectAndForwardPreIntrinsics<TraceThreadListener,
                                llvm::Intrinsic::ID::memcpy,
                                llvm::Intrinsic::ID::memmove,
                                llvm::Intrinsic::ID::memset>
                                (*this, CI, Index, ID);

  switch (ID) {
    case llvm::Intrinsic::ID::stackrestore:
      GlobalMemoryLock = ProcessListener.lockMemory();
    default:
      break;
  }
}

void TraceThreadListener::notifyPostCallIntrinsic(uint32_t Index,
                                                  llvm::CallInst const *CI) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPostNotification();});

  auto Function = CI->getCalledFunction();
  auto ID = Function->getIntrinsicID();

  if (detectAndForwardPostIntrinsics<TraceThreadListener,
                                     llvm::Intrinsic::ID::memcpy,
                                     llvm::Intrinsic::ID::memmove,
                                     llvm::Intrinsic::ID::memset>
                                     (*this, CI, Index, ID)) {
    return;
  }
  
  auto ActiveFunc = getActiveFunction();
  assert(ActiveFunc && "No active function!");

  switch (ID) {
    case llvm::Intrinsic::ID::stacksave:
    {
      auto SaveRTV = getCurrentRuntimeValueAs<uintptr_t>(*this, CI);
      assert(SaveRTV.assigned() && "Couldn't get stacksave run-time value.");

      ActiveFunc->stackSave(SaveRTV.get<0>());

      break;
    }

    case llvm::Intrinsic::ID::stackrestore:
    {
      auto SaveValue = CI->getArgOperand(0);
      auto SaveRTV = getCurrentRuntimeValueAs<uintptr_t>(*this, SaveValue);
      assert(SaveRTV.assigned() && "Couldn't get stacksave run-time value.");

      auto Cleared = ActiveFunc->stackRestore(SaveRTV.get<0>());
      if (Cleared.length()) {
        recordStateClear(Cleared.address(), Cleared.length());
      }

      EventsOut.write<EventType::Instruction>(Index, ++Time);
      
      // Write StackRestore event.
      EventsOut.write<EventType::StackRestore>
                     (EventsOut.getPreviousOffsetOf(EventType::StackRestore));
      
      // Write StackRestoreAlloca events.
      for (auto const &Alloca : ActiveFunc->getAllocas()) {
        EventsOut.write<EventType::StackRestoreAlloca>(Alloca.eventOffset());
      }

      break;
    }

    default:
      break;
  }
}

void TraceThreadListener::notifyPreAlloca(uint32_t const Index,
                                          llvm::AllocaInst const &Alloca,
                                          uint64_t const ElemSize,
                                          uint64_t const ElemCount)
{
  auto const Remaining = getRemainingStack();
  if (Remaining / ElemSize < ElemCount) {
    handleRunError(
      *runtime_errors::createRunError
        <runtime_errors::RunErrorType::StackOverflowAlloca>(0),
      RunErrorSeverity::Fatal,
      Index);
  }
}

void TraceThreadListener::notifyPreLoad(uint32_t Index,
                                        llvm::LoadInst const *Load,
                                        void const *Data,
                                        std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPreNotification();});
  ActiveFunction->setActiveInstruction(Load);

  GlobalMemoryLock = ProcessListener.lockMemory();

  auto const Address = reinterpret_cast<uintptr_t>(Data);
  auto const Access = seec::runtime_errors::format_selects::MemoryAccess::Read;
  
  RuntimeErrorChecker Checker(*this, Index);
  auto const MaybeArea = seec::trace::getContainingMemoryArea(*this, Address);
  auto const Obj = ActiveFunction->getPointerObject(Load->getPointerOperand());

  Checker.checkPointer(Obj, Address, MaybeArea);
  Checker.memoryExists(Address, Size, Access, MaybeArea);
  Checker.checkMemoryAccess(Address, Size, Access, MaybeArea.get<0>());
}

void TraceThreadListener::notifyPostLoad(uint32_t Index,
                                         llvm::LoadInst const *Load,
                                         void const *Address,
                                         std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPostNotification();});

  if (Load->getType()->isPointerTy()) {
    auto const AddressInt = reinterpret_cast<uintptr_t>(Address);
    auto const Origin = ProcessListener.getInMemoryPointerObject(AddressInt);
    if (Origin)
      ActiveFunction->setPointerObject(Load, Origin);
  }
}

void TraceThreadListener::notifyPreStore(uint32_t Index,
                                         llvm::StoreInst const *Store,
                                         void const *Data,
                                         std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPreNotification();});
  ActiveFunction->setActiveInstruction(Store);

  GlobalMemoryLock = ProcessListener.lockMemory();

  auto const Address = reinterpret_cast<uintptr_t>(Data);
  auto const Access = seec::runtime_errors::format_selects::MemoryAccess::Write;

  RuntimeErrorChecker Checker(*this, Index);
  auto const MaybeArea = seec::trace::getContainingMemoryArea(*this, Address);
  auto const Obj = ActiveFunction->getPointerObject(Store->getPointerOperand());

  Checker.checkPointer(Obj, Address, MaybeArea);
  Checker.memoryExists(Address, Size, Access, MaybeArea);
  Checker.checkMemoryAccess(Address, Size, Access, MaybeArea.get<0>());
}

void TraceThreadListener::notifyPostStore(uint32_t Index,
                                          llvm::StoreInst const *Store,
                                          void const *Address,
                                          std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPostNotification();});
  
  EventsOut.write<EventType::Instruction>(Index, ++Time);

  auto StoreValue = Store->getValueOperand();

  // Set the in-memory pointer's origin information.
  if (StoreValue->getType()->isPointerTy()) {
    if (auto const Origin = ActiveFunction->getPointerObject(StoreValue)) {
      auto const AddressInt = reinterpret_cast<uintptr_t>(Address);
      ProcessListener.setInMemoryPointerObject(AddressInt, Origin);
    }
  }

  if (auto StoreValueInst = llvm::dyn_cast<llvm::Instruction>(StoreValue)) {
    auto RTValue = getActiveFunction()->getCurrentRuntimeValue(StoreValueInst);
    assert(RTValue);
    
    recordTypedState(Address, Size, RTValue->getRecordOffset());
  }
  else {
    recordUntypedState(reinterpret_cast<char const *>(Address), Size);
  }
}

template<bool Signed, typename DivisorType>
void checkIntegerDivisor(TraceThreadListener &Listener,
                         llvm::BinaryOperator const *Instruction,
                         uint32_t InstructionIndex,
                         llvm::Value const *Divisor) {
  auto DivisorRTV = getCurrentRuntimeValueAs<DivisorType>(Listener, Divisor);
  if (!DivisorRTV.assigned())
    llvm_unreachable("Couldn't get Divisor run-time value.");

  // Check division-by-zero
  if (!DivisorRTV.template get<0>()) {
    using namespace seec::runtime_errors;

    Listener.handleRunError(*createRunError<RunErrorType::DivideByZero>
                                           (ArgObject{}),
                            RunErrorSeverity::Fatal,
                            InstructionIndex);
  }

  // Check for integer overflow
  if (Signed) {
    // TODO
  }
}

template<bool Signed>
void checkIntegerDivision(TraceThreadListener &Listener,
                          llvm::BinaryOperator const *Instruction,
                          uint32_t InstructionIndex) {
  auto Divisor = Instruction->getOperand(1);
  auto DivisorTy = llvm::dyn_cast<llvm::IntegerType>(Divisor->getType());

  assert(DivisorTy && "Expected integer divisor type.");

  auto BitWidth = DivisorTy->getBitWidth();

  if (BitWidth <= 8)
    checkIntegerDivisor<Signed, uint8_t>(Listener, Instruction,
                                         InstructionIndex, Divisor);
  else if (BitWidth <= 16)
    checkIntegerDivisor<Signed, uint16_t>(Listener, Instruction,
                                          InstructionIndex, Divisor);
  else if (BitWidth <= 32)
    checkIntegerDivisor<Signed, uint32_t>(Listener, Instruction,
                                          InstructionIndex, Divisor);
  else if (BitWidth <= 64)
    checkIntegerDivisor<Signed, uint64_t>(Listener, Instruction,
                                          InstructionIndex, Divisor);
  else
    llvm_unreachable("Unsupported integer bitwidth.");
}

template<typename DivisorType>
void checkFloatDivisor(TraceThreadListener &Listener,
                       llvm::BinaryOperator const *Instruction,
                       uint32_t InstructionIndex,
                       llvm::Value const *Divisor) {
  auto DivisorRTV = getCurrentRuntimeValueAs<DivisorType>(Listener, Divisor);
  if (!DivisorRTV.assigned())
    llvm_unreachable("Couldn't get Divisor run-time value.");

  // Check division-by-zero
  if (!DivisorRTV.template get<0>()) {
    using namespace seec::runtime_errors;

    Listener.handleRunError(*createRunError<RunErrorType::DivideByZero>
                                           (ArgObject{}),
                            RunErrorSeverity::Fatal,
                            InstructionIndex);
  }
}

void checkFloatDivision(TraceThreadListener &Listener,
                        llvm::BinaryOperator const *Instruction,
                        uint32_t InstructionIndex) {
  auto Divisor = Instruction->getOperand(1);
  auto DivisorType = Divisor->getType();

  if (DivisorType->isFloatTy())
    checkFloatDivisor<float>(Listener, Instruction, InstructionIndex, Divisor);
  else if (DivisorType->isDoubleTy())
    checkFloatDivisor<double>(Listener, Instruction, InstructionIndex, Divisor);
  else
    llvm_unreachable("Unsupported divisor type.");
}

void TraceThreadListener::notifyPreDivide(
                            uint32_t Index,
                            llvm::BinaryOperator const *Instruction) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPreNotification();});

  // Check that the division is safe
  switch (Instruction->getOpcode()) {
    case llvm::Instruction::BinaryOps::UDiv: SEEC_FALLTHROUGH;
    case llvm::Instruction::BinaryOps::URem:
      checkIntegerDivision<false>(*this, Instruction, Index);
      break;
    case llvm::Instruction::BinaryOps::SDiv: SEEC_FALLTHROUGH;
    case llvm::Instruction::BinaryOps::SRem:
      checkIntegerDivision<true>(*this, Instruction, Index);
      break;
    case llvm::Instruction::BinaryOps::FDiv: SEEC_FALLTHROUGH;
    case llvm::Instruction::BinaryOps::FRem:
      checkFloatDivision(*this, Instruction, Index);
      break;
    default:
      break;
  }
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      void *Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);
  
  auto const IntVal = reinterpret_cast<uintptr_t>(Value);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>(
                                  Index,
                                  ++Time,
                                  RTValue.getRecordOffset(),
                                  RuntimeValueRecord{Value});

  RTValue.set(Offset, IntVal);

  if (auto Alloca = llvm::dyn_cast<llvm::AllocaInst>(Instruction)) {
    // Add a record to this function's stack.
    auto AllocaType = Alloca->getAllocatedType();

    auto &DataLayout = ProcessListener.dataLayout();

    auto const ElementSize = DataLayout.getTypeAllocSize(AllocaType);

    auto const CountRTV = getCurrentRuntimeValueAs<std::size_t>
                                                  (*this,
                                                   Alloca->getArraySize());
    assert(CountRTV.assigned() && "Couldn't get Count run-time value.");
    
    auto const Offset = EventsOut.write<EventType::Alloca>(ElementSize,
                                                           CountRTV.get<0>());

    ActiveFunction->addAlloca(TracedAlloca(Alloca,
                                           IntVal,
                                           ElementSize,
                                           CountRTV.get<0>(),
                                           Offset));

    // Origin of the pointer will be this alloca.
    ActiveFunction->setPointerObject(Instruction, IntVal);
  }
  else if (auto Cast = llvm::dyn_cast<llvm::BitCastInst>(Instruction)) {
    ActiveFunction->transferPointerObject(Cast->getOperand(0), Cast);
  }
  else if (llvm::isa<llvm::LoadInst>(Instruction)) {
    // Handled in PostLoad.
  }
  else if (auto GEP = llvm::dyn_cast<llvm::GetElementPtrInst>(Instruction)) {
    // Set the origin to the origin of the base pointer.
    auto const Base = GEP->getPointerOperand();
    auto const Origin = ActiveFunction->getPointerObject(Base);
    if (Origin)
      ActiveFunction->setPointerObject(GEP, Origin);
  }
  else if (llvm::isa<llvm::CallInst>(Instruction)) {
    // Should be handled by interceptor / detect calls.
  }
  else if (Instruction->getType()->isPointerTy()) {
    llvm::errs() << "don't know how to set origin for pointer Instruction:\n"
                 << *Instruction << "\n";
  }
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint64_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord{Value});

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint32_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord{Value});

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint16_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithSmallValue>
                               (Value,
                                Index,
                                ++Time,
                                RTValue.getRecordOffset());

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint8_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithSmallValue>
                               (Value,
                                Index,
                                ++Time,
                                RTValue.getRecordOffset());

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      float Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord{Value});

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      double Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord{Value});

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      long double Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord{Value});
  
  RTValue.set(Offset, Value);
}

} // namespace trace (in seec)

} // namespace seec
