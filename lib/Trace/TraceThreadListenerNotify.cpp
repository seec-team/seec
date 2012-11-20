#include "TraceThreadMemCheck.hpp"

#include "seec/Trace/DetectCalls.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/CheckNew.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/Util/SynchronizedExit.hpp"

#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Type.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

void TraceThreadListener::enterNotification() {
  synchronizeProcessTime();
}

void TraceThreadListener::exitNotification() {
  SupportSyncExit.getSynchronizedExit().check();
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
  
  clearCI();
  
  exitNotification();
}

void TraceThreadListener::notifyFunctionBegin(uint32_t Index,
                                              llvm::Function const *F) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});

  uint64_t Entered = ++Time;

  // Find the location that the new FunctionRecord will be placed at.
  auto RecordOffset = getNewFunctionRecordOffset();

  // Create function start event.
  auto StartOffset = EventsOut.write<EventType::FunctionStart>(RecordOffset);

  // Get the shared, indexed view of the function.
  auto const &FIndex = ProcessListener.moduleIndex().getFunctionIndex(Index);

  // create function record
  auto TF = new (std::nothrow) TracedFunction(*FIndex,
                                              RecordOffset,
                                              Index,
                                              StartOffset,
                                              Entered);
  seec::checkNew(TF);

  RecordedFunctions.emplace_back(TF);

  // Add the FunctionRecord to the stack and make it the ActiveFunction.
  {
    std::lock_guard<std::mutex> Lock(FunctionStackMutex);

    // If there was already an active function, add the new function as a child,
    // otherwise record it as a new top-level function.
    if (!FunctionStack.empty()) {
      FunctionStack.back()->addChild(*TF);
    }
    else {
      RecordedTopLevelFunctions.emplace_back(RecordOffset);
    }

    FunctionStack.push_back(TF);

    ActiveFunction = TF;
  }
}

void TraceThreadListener::notifyArgs(uint64_t ArgC, char **ArgV) {
  // Handle common behaviour when entering and exiting notifications.
  // Note thatnotifyArgs has the exit behaviour of a post-notification, because
  // it effectively ends the FunctionStart block for main().
  enterNotification();
  ScopeExit OnExit([=](){exitPostNotification();});
  
  GlobalMemoryLock = ProcessListener.lockMemory();
  
  // Make the pointer array read/write.
  auto TableAddress = reinterpret_cast<uintptr_t>(ArgV);
  auto TableSize = sizeof(char *[ArgC + 1]);
  ProcessListener.addKnownMemoryRegion(TableAddress,
                                       TableSize,
                                       MemoryPermission::ReadWrite);
  
  // Set the state of the pointer array.
  recordUntypedState(reinterpret_cast<char const *>(ArgV), TableSize);
  
  // Now each of the individual strings.
  for (uint64_t i = 0; i < ArgC; ++i) {
    // Make the string read/write.
    auto StringAddress = reinterpret_cast<uintptr_t>(ArgV[i]);
    auto StringSize = strlen(ArgV[i]) + 1;
    ProcessListener.addKnownMemoryRegion(StringAddress,
                                         StringSize,
                                         MemoryPermission::ReadWrite);
    
    // Set the state of the string.
    recordUntypedState(ArgV[i], StringSize);
  }

  GlobalMemoryLock.unlock();
}

void TraceThreadListener::notifyEnv(char **EnvP) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});
  
  GlobalMemoryLock = ProcessListener.lockMemory();
  
  // Find the number of pointers in the array (including the NULL pointer that
  // terminates the array).
  std::size_t Count = 0;
  while (EnvP[Count++]);
  
  // Make the pointer array readable.
  auto TableAddress = reinterpret_cast<uintptr_t>(EnvP);
  auto TableSize = sizeof(char *[Count]);
  ProcessListener.addKnownMemoryRegion(TableAddress,
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
    ProcessListener.addKnownMemoryRegion(StringAddress,
                                         StringSize,
                                         MemoryPermission::ReadOnly);
    
    // Set the state of the string.
    recordUntypedState(EnvP[i], StringSize);
  }
  
  GlobalMemoryLock.unlock();
}

void TraceThreadListener::notifyFunctionEnd(uint32_t Index,
                                            llvm::Function const *F) {
  // It's OK to check this without owning FunctionStackMutex, because the
  // FunctionStack can only be changed by a single thread.
  assert(!FunctionStack.empty() && "notifyFunctionEnd with empty stack.");

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});

  uint64_t Exited = ++Time;
  TracedFunction *TF;

  // Get the FunctionRecord from the stack and update ActiveFunction.
  {
    std::lock_guard<std::mutex> Lock(FunctionStackMutex);

    TF = FunctionStack.back();
    FunctionStack.pop_back();

    ActiveFunction = FunctionStack.empty() ? nullptr : FunctionStack.back();
  }

  // Create function end event.
  auto EndOffset =
    EventsOut.write<EventType::FunctionEnd>(TF->getRecordOffset());

  // Clear function's stack range.
  GlobalMemoryLock = ProcessListener.lockMemory();

  auto StackArea = TF->getStackArea();
  recordStateClear(StackArea.address(), StackArea.length());

  GlobalMemoryLock.unlock();

  // Update the function record with the end details.
  TF->finishRecording(EndOffset, Exited);
}

void TraceThreadListener::notifyPreCall(uint32_t Index,
                                        llvm::CallInst const *CallInst,
                                        void const *Address) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPreNotification();});

  auto const &Lookup = ProcessListener.getDetectCallsLookup();

  detectPreCalls<TraceThreadListener,
                 // stdio.h
                 Call::Cfopen,
                 Call::Cfreopen,
                 Call::Cfclose,
                 Call::Cfflush,
                 Call::Cfwide,
                 // stdlib.h
                 Call::Cstdlib_h_string,
                 Call::Cstdlib_h_memory,
                 Call::Cgetenv,
                 Call::Csystem,
                 // string.h
                 Call::Cmemchr,
                 Call::Cmemcmp,
                 Call::Cmemcpy,
                 Call::Cmemmove,
                 Call::Cmemset,
                 Call::Cstrcat,
                 Call::Cstrchr,
                 Call::Cstrcmp,
                 Call::Cstrcoll,
                 Call::Cstrcpy,
                 Call::Cstrcspn,
                 Call::Cstrerror,
                 Call::Cstrlen,
                 Call::Cstrncat,
                 Call::Cstrncpy,
                 Call::Cstrpbrk,
                 Call::Cstrrchr,
                 Call::Cstrspn,
                 Call::Cstrstr,
                 Call::Cstrtok,
                 Call::Cstrxfrm>
                 (Lookup, *this, CallInst, Index, Address);
}

void TraceThreadListener::notifyPostCall(uint32_t Index,
                                         llvm::CallInst const *CallInst,
                                         void const *Address) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPostNotification();});

  auto const &Lookup = ProcessListener.getDetectCallsLookup();

  detectPostCalls<TraceThreadListener,
                  // stdio.h
                  Call::Cfopen,
                  Call::Cfreopen,
                  Call::Cfclose,
                  // stdlib.h
                  Call::Cstrtol,
                  Call::Cstrtoll,
                  Call::Cstrtoul,
                  Call::Cstrtoull,
                  Call::Cstrtof,
                  Call::Cstrtod,
                  Call::Cstrtold,
                  Call::Cstrtoimax,
                  Call::Cstrtoumax,
                  Call::Cstdlib_h_memory,
                  Call::Cgetenv,
                  // string.h
                  Call::Cmemcmp,
                  Call::Cmemcpy,
                  Call::Cmemmove,
                  Call::Cmemset,
                  Call::Cstrcat,
                  Call::Cstrcpy,
                  Call::Cstrerror,
                  Call::Cstrncat,
                  Call::Cstrncpy,
                  Call::Cstrtok,
                  Call::Cstrxfrm>
                  (Lookup, *this, CallInst, Index, Address);
}

void TraceThreadListener::notifyPreCallIntrinsic(uint32_t Index,
                                                 llvm::CallInst const *CI) {
  using namespace seec::trace::detect_calls;

  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPreNotification();});

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
  ScopeExit OnExit([=](){exitPostNotification();});

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

void TraceThreadListener::notifyPreLoad(uint32_t Index,
                                        llvm::LoadInst const *Load,
                                        void const *Data,
                                        std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPreNotification();});

  GlobalMemoryLock = ProcessListener.lockMemory();

  auto const Address = reinterpret_cast<uintptr_t>(Data);
  auto const Access = seec::runtime_errors::format_selects::MemoryAccess::Read;
  
  RuntimeErrorChecker Checker(*this, Index);
  Checker.checkMemoryExistsAndAccessible(Address, Size, Access);
}

void TraceThreadListener::notifyPostLoad(uint32_t Index,
                                         llvm::LoadInst const *Load,
                                         void const *Address,
                                         std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPostNotification();});

  // auto MemoryStateEvent = ProcessListener.getMemoryStateEvent();
}

void TraceThreadListener::notifyPreStore(uint32_t Index,
                                         llvm::StoreInst const *Store,
                                         void const *Address,
                                         std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPreNotification();});

  GlobalMemoryLock = ProcessListener.lockMemory();

  auto const AddressInt = reinterpret_cast<uintptr_t>(Address);
  auto const Access = seec::runtime_errors::format_selects::MemoryAccess::Write;
  
  RuntimeErrorChecker Checker(*this, Index);
  Checker.checkMemoryExistsAndAccessible(AddressInt, Size, Access);
}

void TraceThreadListener::notifyPostStore(uint32_t Index,
                                          llvm::StoreInst const *Store,
                                          void const *Address,
                                          std::size_t Size) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitPostNotification();});
  
  EventsOut.write<EventType::Instruction>(Index, ++Time);

  auto StoreValue = Store->getValueOperand();

  if (auto StoreValueInst = llvm::dyn_cast<llvm::Instruction>(StoreValue)) {
    auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(StoreValueInst);
    recordTypedState(Address, Size, RTValue.getRecordOffset());
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

    Listener.handleRunError(
      createRunError<RunErrorType::DivideByZero>(ArgObject{}),
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

    Listener.handleRunError(
      createRunError<RunErrorType::DivideByZero>(ArgObject{}),
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
  ScopeExit OnExit([=](){exitPreNotification();});

  // Check that the division is safe
  switch (Instruction->getOpcode()) {
    case llvm::Instruction::BinaryOps::UDiv: // [[clang::fallthrough]];
    case llvm::Instruction::BinaryOps::URem:
      checkIntegerDivision<false>(*this, Instruction, Index);
      break;
    case llvm::Instruction::BinaryOps::SDiv: // [[clang::fallthrough]];
    case llvm::Instruction::BinaryOps::SRem:
      checkIntegerDivision<true>(*this, Instruction, Index);
      break;
    case llvm::Instruction::BinaryOps::FDiv: // [[clang::fallthrough]];
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
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

  auto IntVal = reinterpret_cast<uintptr_t>(Value);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>(
                                  Index,
                                  ++Time,
                                  RTValue.getRecordOffset(),
                                  RuntimeValueRecord::ofPointer(IntVal));

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

    getActiveFunction()->addAlloca(TracedAlloca(Alloca,
                                                IntVal,
                                                ElementSize,
                                                CountRTV.get<0>(),
                                                Offset));
  }
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint64_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                Value);

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint32_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                Value);

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      uint16_t Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

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
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

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
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord(Value));

  RTValue.set(Offset, Value);
}

void TraceThreadListener::notifyValue(uint32_t Index,
                                      llvm::Instruction const *Instruction,
                                      double Value) {
  // Handle common behaviour when entering and exiting notifications.
  enterNotification();
  ScopeExit OnExit([=](){exitNotification();});

  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Index);

  auto Offset = EventsOut.write<EventType::InstructionWithValue>
                               (Index,
                                ++Time,
                                RTValue.getRecordOffset(),
                                RuntimeValueRecord(Value));

  RTValue.set(Offset, Value);
}

} // namespace trace (in seec)

} // namespace seec
