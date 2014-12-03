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

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
extern "C" {
  extern char **environ;
}
#endif

namespace seec {

namespace trace {

void TraceThreadListener::enterNotification() {
  synchronizeProcessTime();
  checkSignals();
}

void TraceThreadListener::exitNotification() {}

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

  // We can't hold any locks when we do a synchronized exit, so we only check
  // for synchronized exits here (after any locks have been released).
  SupportSyncExit.getSynchronizedExit().check();
}

void TraceThreadListener::notifyFunctionBegin(uint32_t Index,
                                              llvm::Function const *F) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitPostNotification();});

  uint64_t Entered = ++Time;

  // Find the location that the new FunctionRecord will be placed at.
  auto RecordOffset = getNewFunctionRecordOffset();

  // Create function start event.
  auto StartOffset = EventsOut.write<EventType::FunctionStart>(RecordOffset);

  // Get the shared, indexed view of the function.
  auto const &FIndex = ProcessListener.moduleIndex().getFunctionIndex(Index);

  // Get object information for Arguments from the call site.
  llvm::DenseMap<llvm::Argument const *, PointerTarget> PtrArgObjects;

  if (ActiveFunction) {
    if (!ActiveFunction->isShim()) {
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
    else {
      // A shim's Argument lookup finds the called Function's argument pointer
      // objects, rather than the shim's argument pointer objects.
      for (auto const &Arg : F->getArgumentList())
        if (Arg.getType()->isPointerTy())
          PtrArgObjects[&Arg] = ActiveFunction->getPointerObject(&Arg);
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
    auto const PriorStackSize = FunctionStack.size();

    FunctionStack.emplace_back(*this,
                               *FIndex,
                               *RecordedFunctions.back(),
                               std::move(PtrArgObjects));

    auto const Parent = PriorStackSize ? &(FunctionStack[PriorStackSize-1])
                                       : nullptr;

    ActiveFunction = &(FunctionStack.back());

    // If there was already an active function, add the new function as a child,
    // otherwise record it as a new top-level function.
    if (Parent)
      Parent->addChild(*ActiveFunction);
    else
      RecordedTopLevelFunctions.emplace_back(RecordOffset);

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
    if (!Parent && RecordedTopLevelFunctions.size() == 1 && environ) {
      acquireGlobalMemoryWriteLock();

      // Record the environ table (if it hasn't been done already).
      setupEnvironTable(environ);
      auto const EnvironValue = reinterpret_cast<uintptr_t>(environ);

      // Update the GVIMPO.
      ProcessListener.setInMemoryPointerObject(
        reinterpret_cast<uintptr_t>(&environ),
        ProcessListener.makePointerObject(EnvironValue));
    }
#endif
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
  
  auto const &DataLayout = ProcessListener.getDataLayout();
  auto const PointeeSize = DataLayout.getTypeStoreSize(PointeeType);
  
  // Record this memory area in the trace.
  EventsOut.write<EventType::ByValRegionAdd>(Arg->getArgNo(),
                                             AddressInt,
                                             PointeeSize);
  
  // Lock global memory, and release when we exit scope.
  acquireGlobalMemoryWriteLock();
  auto UnlockGlobalMemory = scopeExit([=](){ GlobalMemoryLock.unlock(); });
  
  // Add the memory area of the argument. We must increment the region's ID
  // first, because addByValArg will associate it with the llvm::Argument *.
  ProcessListener.incrementRegionTemporalID(AddressInt);
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
  
  GlobalMemoryLock = ProcessListener.lockMemory();
  
  // Make the pointer array read/write.
  auto TableAddress = reinterpret_cast<uintptr_t>(ArgV);
  auto TableSize = sizeof(char *[ArgC + 1]);
  
  addKnownMemoryRegion(TableAddress, TableSize, MemoryPermission::ReadWrite);
  
  // Set the state of the pointer array.
  recordUntypedState(reinterpret_cast<char const *>(ArgV), TableSize);
  
  // Set the object of the argv Argument. This must happen after the target
  // region is set as known, so that the temporal ID is correct.
  auto const ArgVArg = ActiveFunction->getFunctionIndex().getArgument(1);
  auto const ArgVAddr = reinterpret_cast<uintptr_t>(ArgV);
  ActiveFunction->setPointerObject(ArgVArg,
                                   ProcessListener.makePointerObject(ArgVAddr));

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
    ProcessListener.setInMemoryPointerObject(
      PtrLocation,
      ProcessListener.makePointerObject(StringAddress));
  }

  GlobalMemoryLock.unlock();
}

void TraceThreadListener::setupEnvironTable(char **Environ)
{
  assert(GlobalMemoryLock.owns_lock());

  // Find the number of pointers in the array (including the NULL pointer that
  // terminates the array).
  std::size_t Count = 0;
  while (Environ[Count++]);

  // Make the pointer array readable.
  auto TableAddress = reinterpret_cast<uintptr_t>(Environ);
  auto TableSize = sizeof(char *[Count]);

  addKnownMemoryRegion(TableAddress,
                       TableSize,
                       MemoryPermission::ReadWrite);

  // Set the state of the pointer array.
  recordUntypedState(reinterpret_cast<char const *>(Environ), TableSize);

  // Now each of the individual strings. The limit is Count-1 because the final
  // entry in environ is a NULL pointer.
  for (std::size_t i = 0; i < Count - 1; ++i) {
    // Make the string readable.
    auto StringAddress = reinterpret_cast<uintptr_t>(Environ[i]);
    auto StringSize = strlen(Environ[i]) + 1;

    addKnownMemoryRegion(StringAddress,
                         StringSize,
                         MemoryPermission::ReadOnly);

    // Set the state of the string.
    recordUntypedState(Environ[i], StringSize);

    // Set the target of the pointer.
    auto const PtrLocation = reinterpret_cast<uintptr_t>(Environ + i);
    ProcessListener.setInMemoryPointerObject(
      PtrLocation,
      ProcessListener.makePointerObject(StringAddress));
  }
}

void TraceThreadListener::notifyEnv(char **EnvP) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  GlobalMemoryLock = ProcessListener.lockMemory();

  // NOTE: The environ table setup is called from notifyFunctionBegin, so
  // that it happens regardless of whether or not envp is specified, as it may
  // be accessed through an "extern char **environ" declaration.

  // Set the object of the envp Argument. This must happen after the target
  // region is set to known, so that the temporal ID is correct.
  auto const EnvPArg = ActiveFunction->getFunctionIndex().getArgument(2);
  auto const EnvPAddr = reinterpret_cast<uintptr_t>(EnvP);
  ActiveFunction->setPointerObject(EnvPArg,
                                   ProcessListener.makePointerObject(EnvPAddr));

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
  if (ParentFunction
      && F->getReturnType()->isPointerTy()
      && !ParentFunction->isShim())
  {
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
                                        std::size_t Size)
{
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

  Checker.checkPointer(Obj, Address);
  Checker.memoryExists(Address, Size, Access, MaybeArea);

  // Only check memory access for individual members of struct types.
  if (auto StructTy = llvm::dyn_cast<llvm::StructType>(Load->getType())) {
    auto const &DL = getDataLayout();
    llvm::SmallVector<std::pair<llvm::StructType *, uintptr_t>, 1> Elems;
    Elems.push_back(std::make_pair(StructTy, Address));

    while (!Elems.empty()) {
      auto const Elem = Elems.pop_back_val();
      auto const NumChildren = Elem.first->getNumElements();
      auto const Layout = DL.getStructLayout(Elem.first);

      for (unsigned i = 0; i < NumChildren; ++i) {
        auto const ElemAddr = Elem.second + Layout->getElementOffset(i);
        auto const ElemType = Elem.first->getElementType(i);

        if (auto const STy = llvm::dyn_cast<llvm::StructType>(ElemType)) {
          Elems.push_back(std::make_pair(STy, ElemAddr));
        }
        else {
          Checker.checkMemoryAccess(ElemAddr,
                                    DL.getTypeStoreSize(ElemType),
                                    Access,
                                    MaybeArea.get<0>());
        }
      }
    }
  }
  else {
    Checker.checkMemoryAccess(Address, Size, Access, MaybeArea.get<0>());
  }
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

  Checker.checkPointer(Obj, Address);
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

void TraceThreadListener::notifyValue(uint32_t const Index,
                                      llvm::Instruction const * const Instr)
{
  enterNotification();
  auto OnExit = scopeExit([this](){exitNotification();});

  ActiveFunction->setActiveInstruction(Instr);

  EventsOut.write<EventType::Instruction>(Index, ++Time);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      void *Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *(ActiveFunction->getCurrentRuntimeValue(Index));
  ActiveFunction->setActiveInstruction(Instruction);

  auto Offset = EventsOut.write<EventType::InstructionWithPtr>(
                                  Index,
                                  ++Time,
                                  RTValue.getRecordOffset(),
                                  reinterpret_cast<uintptr_t>(Value));

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  auto const IntVal = reinterpret_cast<uintptr_t>(Value);
  RTValue.set(Offset, IntVal);

  if (auto Alloca = llvm::dyn_cast<llvm::AllocaInst>(Instruction)) {
    // Add a record to this function's stack.
    auto AllocaType = Alloca->getAllocatedType();

    auto &DataLayout = ProcessListener.getDataLayout();

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

    ProcessListener.incrementRegionTemporalID(IntVal);

    // Origin of the pointer will be this alloca.
    ActiveFunction->setPointerObject(Instruction,
                                     ProcessListener.makePointerObject(IntVal));
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
    if (Origin) {
      ActiveFunction->setPointerObject(GEP, Origin);

      // Check that this region has not been deallocated and reallocated since
      // the pointer was created.
      auto const PtrBase = Origin.getBase();
      auto const CurrentTime = ProcessListener.getRegionTemporalID(PtrBase);
      if (CurrentTime != Origin.getTemporalID()) {
        handleRunError(*runtime_errors::createRunError
          <runtime_errors::RunErrorType::PointerArithmeticOperandOutdated>
          (Origin.getTemporalID(), CurrentTime),
          RunErrorSeverity::Fatal);
      }

      // Check that the new pointer points to the same object or to the one past
      // the end of the same object.
      auto const MaybeArea = trace::getContainingMemoryArea(*this, PtrBase);
      if (MaybeArea.assigned<seec::MemoryArea>()) {
        auto const &Area = MaybeArea.get<seec::MemoryArea>();
        if (!Area.contains(IntVal) && Area.end() != IntVal) {
          handleRunError(*runtime_errors::createRunError
            <runtime_errors::RunErrorType::PointerArithmeticResultInvalid>
            (Origin.getBase(), IntVal),
            RunErrorSeverity::Fatal);
        }
      }
      else {
        // Raise an error for manipulating a pointer that does not point to a
        // valid object.
        handleRunError(*runtime_errors::createRunError
          <runtime_errors::RunErrorType::PointerArithmeticOperandInvalid>
          (Origin.getBase()),
          RunErrorSeverity::Fatal);
      }
    }
    else {
      // Raise an error for manipulating a NULL pointer.
      handleRunError(*runtime_errors::createRunError
        <runtime_errors::RunErrorType::PointerArithmeticOperandInvalid>
        (Origin.getBase()),
        RunErrorSeverity::Fatal);
    }
  }
  else if (llvm::isa<llvm::CallInst>(Instruction)) {
    // Should be handled by interceptor / detect calls.
  }
  else if (auto const Phi = llvm::dyn_cast<llvm::PHINode>(Instruction)) {
    auto const PreviousBB = ActiveFunction->getPreviousBasicBlock();
    auto const Incoming = Phi->getIncomingValueForBlock(PreviousBB);

    if (Incoming) {
      auto const PtrObject = ActiveFunction->getPointerObject(Incoming);
      ActiveFunction->setPointerObject(Instruction, PtrObject);
    }
    else {
      llvm::errs() << "no incoming value for phi node:\n"
                   << *Instruction << "\n";
    }
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

  auto Offset = EventsOut.write<EventType::InstructionWithUInt64>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                Value);

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint32_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithUInt32>
                               (Value,
                                Index,
                                ++Time,
                                RTValue.getRecordOffset());

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint16_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithUInt16>
                               (Value,
                                Index,
                                ++Time,
                                RTValue.getRecordOffset());

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint8_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithUInt8>
                               (Value,
                                Index,
                                ++Time,
                                RTValue.getRecordOffset());

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      float Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithFloat>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                Value);

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      double Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithDouble>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                Value);

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      long double Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  auto OnExit = scopeExit([=](){exitNotification();});

  auto &RTValue = *getActiveFunction()->getCurrentRuntimeValue(Index);

  uint64_t Words[2] = {0, 0};

  static_assert(sizeof(Value) <= sizeof(Words), "long double too large!");
  memcpy(reinterpret_cast<char *>(Words),
         reinterpret_cast<char const *>(&Value),
         sizeof(Value));

  auto Offset = EventsOut.write<EventType::InstructionWithLongDouble>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                Words[0], Words[1]);

  // Ensure that RTValues are still valid when tracing is disabled.
  if (!OutputEnabled)
    Offset = 0;

  RTValue.set(Offset, Value);
}

} // namespace trace (in seec)

} // namespace seec
