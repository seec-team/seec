//===- lib/Trace/FunctionState.cpp ----------------------------------------===//
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

#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {


//===------------------------------------------------------------------------===
// AllocaState
//===------------------------------------------------------------------------===

llvm::AllocaInst const *AllocaState::getInstruction() const {
  auto &Lookup = Parent->getFunctionLookup();
  auto Inst = Lookup.getInstruction(InstructionIndex);
  assert(Inst && llvm::isa<llvm::AllocaInst>(Inst));
  return llvm::cast<llvm::AllocaInst>(Inst);
}

MemoryStateRegion AllocaState::getMemoryRegion() const {
  auto &Thread = Parent->getParent();
  auto &Process = Thread.getParent();
  auto &Memory = Process.getMemory();
  return Memory.getRegion(MemoryArea(Address, getTotalSize()));
}


//===------------------------------------------------------------------------===
// RuntimeErrorState
//===------------------------------------------------------------------------===

llvm::Instruction const *RuntimeErrorState::getInstruction() const {
  auto Index = Parent.getFunctionLookup();
  return Index.getInstruction(InstructionIndex);
}

bool RuntimeErrorState::isActive() const {
  return ThreadTime == getParent().getParent().getThreadTime();
}


//===------------------------------------------------------------------------===
// FunctionState
//===------------------------------------------------------------------------===

FunctionState::FunctionState(ThreadState &Parent,
                             uint32_t Index,
                             FunctionIndex const &Function,
                             FunctionTrace Trace)
: Parent(&Parent),
  FunctionLookup(Parent.getParent().getModule().getFunctionIndex(Index)),
  Index(Index),
  Trace(Trace),
  ActiveInstruction(),
  ActiveInstructionComplete(false),
  ValuesUInt64(),
  ValuesPtr(),
  ValuesFloat(),
  ValuesDouble(),
  ValuesAPFloat(),
  Allocas(),
  ParamByVals(),
  RuntimeErrors()
{
  assert(FunctionLookup);
}

llvm::Function const *FunctionState::getFunction() const {
  return Parent->getParent().getModule().getFunction(Index);
}

std::size_t FunctionState::getInstructionCount() const {
  return FunctionLookup->getInstructionCount();
}

llvm::Instruction const *FunctionState::getInstruction(uint32_t Index) const {
  return FunctionLookup->getInstruction(Index);
}

llvm::Instruction const *FunctionState::getActiveInstruction() const {
  if (!ActiveInstruction.assigned())
    return nullptr;

  return FunctionLookup->getInstruction(ActiveInstruction.get<0>());
}

seec::Maybe<MemoryArea>
FunctionState::getContainingMemoryArea(stateptr_ty Address) const {
  auto const Alloca = getAllocaContaining(Address);
  if (Alloca)
    return MemoryArea(Alloca->getAddress(), Alloca->getTotalSize());
  
  for (auto const &ParamByVal : ParamByVals)
    if (ParamByVal.getArea().contains(Address))
      return ParamByVal.getArea();
  
  return seec::Maybe<MemoryArea>();
}

void FunctionState::setValueUInt64(llvm::Instruction const *ForInstruction,
                                   offset_uint const Offset,
                                   uint64_t const Value)
{
  ValuesUInt64[ForInstruction] = Value;
}

void FunctionState::setValuePtr(llvm::Instruction const *ForInstruction,
                                offset_uint const Offset,
                                stateptr_ty const Value)
{
  ValuesPtr[ForInstruction] = Value;
}

void FunctionState::setValueFloat(llvm::Instruction const *ForInstruction,
                                  offset_uint const Offset,
                                  float const Value)
{
  ValuesFloat[ForInstruction] = Value;
}

void FunctionState::setValueDouble(llvm::Instruction const *ForInstruction,
                                   offset_uint const Offset,
                                   double const Value)
{
  ValuesDouble[ForInstruction] = Value;
}

void FunctionState::setValueAPFloat(llvm::Instruction const *ForInstruction,
                                    offset_uint const Offset,
                                    llvm::APFloat Value)
{
  auto It = ValuesAPFloat.find(ForInstruction);
  if (It != ValuesAPFloat.end())
    It->second = std::move(Value);
  else
    ValuesAPFloat.insert(std::make_pair(ForInstruction, std::move(Value)));
}

void FunctionState::clearValue(llvm::Instruction const *ForInstruction)
{
  auto const Type = ForInstruction->getType();

  if (Type->isIntegerTy()) {
    ValuesUInt64.erase(ForInstruction);
  }
  else if (Type->isPointerTy()) {
    ValuesPtr.erase(ForInstruction);
  }
  else if (Type->isFloatTy()) {
    ValuesFloat.erase(ForInstruction);
  }
  else if (Type->isDoubleTy()) {
    ValuesDouble.erase(ForInstruction);
  }
  else if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty()) {
    ValuesAPFloat.erase(ForInstruction);
  }
}

bool FunctionState::isDominatedByActive(llvm::Instruction const *Inst) const
{
  if (!ActiveInstruction.assigned<uint32_t>())
    return false;

  auto const MaybeIndex = FunctionLookup->getIndexOfInstruction(Inst);
  if (!MaybeIndex.assigned<uint32_t>())
    return false;

  if (ActiveInstruction.get<uint32_t>() < MaybeIndex.get<uint32_t>())
    return false;

  return true;
}

bool FunctionState::hasValue(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return false;

  auto const Type = ForInstruction->getType();

  if (Type->isIntegerTy()) {
    return ValuesUInt64.count(ForInstruction);
  }
  else if (Type->isPointerTy()) {
    return ValuesPtr.count(ForInstruction);
  }
  else if (Type->isFloatTy()) {
    return ValuesFloat.count(ForInstruction);
  }
  else if (Type->isDoubleTy()) {
    return ValuesDouble.count(ForInstruction);
  }
  else if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty()) {
    return ValuesAPFloat.count(ForInstruction);
  }
  else {
    return false;
  }
}

Maybe<int64_t>
FunctionState::getValueInt64(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return Maybe<int64_t>();

  auto const It = ValuesUInt64.find(ForInstruction);
  if (It != ValuesUInt64.end()) {
    auto const IntTy = llvm::dyn_cast<llvm::IntegerType>
                                     (ForInstruction->getType());

    auto const Value = It->second;

    // If the integer has its sign bit set, then set all higher bits.
    if (Value & IntTy->getSignBit()) {
      return static_cast<int64_t>(Value | ~IntTy->getBitMask());
    }

    return static_cast<int64_t>(Value);
  }

  return Maybe<int64_t>();
}

Maybe<uint64_t>
FunctionState::getValueUInt64(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return Maybe<uint64_t>();

  auto const It = ValuesUInt64.find(ForInstruction);
  return It != ValuesUInt64.end() ? It->second : Maybe<uint64_t>();
}

Maybe<stateptr_ty>
FunctionState::getValuePtr(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return Maybe<stateptr_ty>();

  auto const It = ValuesPtr.find(ForInstruction);
  return It != ValuesPtr.end() ? It->second : Maybe<stateptr_ty>();
}

Maybe<float>
FunctionState::getValueFloat(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return Maybe<float>();

  auto const It = ValuesFloat.find(ForInstruction);
  return It != ValuesFloat.end() ? It->second : Maybe<float>();
}

Maybe<double>
FunctionState::getValueDouble(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return Maybe<double>();

  auto const It = ValuesDouble.find(ForInstruction);
  return It != ValuesDouble.end() ? It->second : Maybe<double>();
}

Maybe<llvm::APFloat>
FunctionState::getValueAPFloat(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return Maybe<llvm::APFloat>();

  auto const It = ValuesAPFloat.find(ForInstruction);
  return It != ValuesAPFloat.end() ? It->second : Maybe<llvm::APFloat>();
}

std::vector<std::reference_wrapper<AllocaState const>>
FunctionState::getVisibleAllocas() const {
  std::vector<std::reference_wrapper<AllocaState const>> RetVal;
  
  if (!ActiveInstruction.assigned(0))
    return RetVal;
  
  auto const ActiveIdx = ActiveInstruction.get<0>();
  
  for (auto const &Alloca : Allocas) {
    auto const Inst = Alloca.getInstruction();
    auto const MaybeIdx = FunctionLookup->getIndexOfDbgDeclareFor(Inst);
    
    // If the index of the llvm.dbg.declare is greater than our active index,
    // then do not show this alloca. If the llvm.dbg.declare is the very next
    // instruction, then we should still show this (hence the ActiveIdx + 1).
    if (MaybeIdx.assigned(0) && MaybeIdx.get<0>() > ActiveIdx + 1)
      continue;
    
    RetVal.emplace_back(Alloca);
  }
  
  return RetVal;
}

auto
FunctionState::getRuntimeErrorsActive() const
-> seec::Range<decltype(RuntimeErrors)::const_iterator>
{
  auto const It = std::find_if(RuntimeErrors.begin(), RuntimeErrors.end(),
                               [] (RuntimeErrorState const &Err) {
                                return Err.isActive();
                               });
  
  return seec::Range<decltype(RuntimeErrors)::const_iterator>
                    (It, RuntimeErrors.end());
}

void
FunctionState::
addRuntimeError(std::unique_ptr<seec::runtime_errors::RunError> Error) {
  assert(ActiveInstruction.assigned(0)
         && "Runtime error with no active instruction.");
  
  RuntimeErrors.emplace_back(*this,
                             ActiveInstruction.get<0>(),
                             std::move(Error),
                             getParent().getThreadTime());
}

void FunctionState::removeLastRuntimeError() {
  assert(!RuntimeErrors.empty() && "No runtime error to remove.");
  
  RuntimeErrors.pop_back();
}

seec::Maybe<seec::MemoryArea>
FunctionState::getParamByValArea(llvm::Argument const *Arg) const
{
  for (auto const &PBV : ParamByVals)
    if (PBV.getArgument() == Arg)
      return PBV.getArea();
  
  return seec::Maybe<seec::MemoryArea>();
}

void FunctionState::addByValArea(unsigned ArgumentNumber,
                                 stateptr_ty Address,
                                 std::size_t Size)
{
  auto const Fn = getFunction();
  assert(ArgumentNumber < Fn->arg_size());
  
  auto ArgIt = Fn->arg_begin();
  std::advance(ArgIt, ArgumentNumber);
  
  ParamByVals.emplace_back(&*ArgIt, MemoryArea(Address, Size));
}

void FunctionState::removeByValArea(stateptr_ty Address)
{
  auto const It = std::find_if(ParamByVals.begin(),
                               ParamByVals.end(),
                               [=] (ParamByValState const &P) {
                                  return P.getArea().contains(Address);
                               });
  
  if (It != ParamByVals.end())
    ParamByVals.erase(It);
}


void printComparable(llvm::raw_ostream &Out, FunctionState const &State)
{
  Out << "  Function [Index=" << State.getIndex() << "]\n";

  Out << "   Allocas:\n";
  for (auto const &Alloca : State.getAllocas()) {
    Out << "    " << Alloca.getInstructionIndex()
        <<  " =[" << Alloca.getElementCount()
        <<    "x" << Alloca.getElementSize()
        <<  "]\n";
  }

  Out << "   Instruction values [Active=";
  if (State.getActiveInstructionIndex().assigned(0))
    Out << State.getActiveInstructionIndex().get<0>();
  else
    Out << "unassigned";
  Out << "]:\n";

  auto const InstructionCount = State.getInstructionCount();
  for (std::size_t i = 0; i < InstructionCount; ++i) {
    auto const Instruction = State.getInstruction(i);
    auto const Type = Instruction->getType();

    if (llvm::isa<llvm::IntegerType>(Type)) {
      auto const UValue = State.getValueUInt64(Instruction);
      if (UValue.assigned<uint64_t>()) {
        auto const SValue = State.getValueInt64(Instruction);
        assert(SValue.assigned<int64_t>());

        Out << "    " << i << " = (int64_t)" << SValue.get< int64_t>()
                           << ", (uint64_t)" << UValue.get<uint64_t>() << "\n";
      }
    }
    else if (Type->isPointerTy()) {
      auto const Value = State.getValuePtr(Instruction);
      if (Value.assigned<stateptr_ty>()) {
        Out << "    " << i << " = \n";
        // TODO: a comparable pointer representation (this requires us to
        //       determine the allocation that a pointer references, and then
        //       display the pointer value relative to that allocation).
      }
    }
    else if (Type->isFloatTy()) {
      auto const Value = State.getValueFloat(Instruction);
      if (Value.assigned<float>()) {
        Out << "    " << i << " = (float)" << Value.get<float>() << "\n";
      }
    }
    else if (Type->isDoubleTy()) {
      auto const Value = State.getValueDouble(Instruction);
      if (Value.assigned<double>()) {
        Out << "    " << i << " = (double)" << Value.get<double>() << "\n";
      }
    }
    else if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty())
    {
      auto const Value = State.getValueAPFloat(Instruction);
      if (Value.assigned<llvm::APFloat>()) {
        llvm::SmallString<32> Buffer;
        Value.get<llvm::APFloat>().toString(Buffer);
        Out << "    " << i << " = (long double)" << Buffer << "\n";
      }
    }
  }
}

/// Print a textual description of a FunctionState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State) {
  Out << "  Function [Index=" << State.getIndex() << "]\n";

  Out << "   Allocas:\n";
  for (auto const &Alloca : State.getAllocas()) {
    Out << "    " << Alloca.getInstructionIndex()
        <<  " =[" << Alloca.getElementCount()
        <<    "x" << Alloca.getElementSize()
        <<  "] @" << Alloca.getAddress()
        << "\n";
  }

  Out << "   Instruction values [Active=";
  if (State.getActiveInstructionIndex().assigned(0))
    Out << State.getActiveInstructionIndex().get<0>();
  else
    Out << "unassigned";
  Out << "]:\n";
  
  auto const InstructionCount = State.getInstructionCount();
  for (std::size_t i = 0; i < InstructionCount; ++i) {
    auto const Instruction = State.getInstruction(i);
    auto const Type = Instruction->getType();
    
    if (llvm::isa<llvm::IntegerType>(Type)) {
      auto const Value = State.getValueUInt64(Instruction);
      if (Value.assigned<uint64_t>()) {
        Out << "    " << i << " = (uint64_t)" << Value.get<uint64_t>() << "\n";
      }
    }
    else if (Type->isPointerTy()) {
      auto const Value = State.getValuePtr(Instruction);
      if (Value.assigned<stateptr_ty>()) {
        Out << "    " << i << " = (? *)" << Value.get<stateptr_ty>() << "\n";
      }
    }
    else if (Type->isFloatTy()) {
      auto const Value = State.getValueFloat(Instruction);
      if (Value.assigned<float>()) {
        Out << "    " << i << " = (float)" << Value.get<float>() << "\n";
      }
    }
    else if (Type->isDoubleTy()) {
      auto const Value = State.getValueDouble(Instruction);
      if (Value.assigned<double>()) {
        Out << "    " << i << " = (double)" << Value.get<double>() << "\n";
      }
    }
    else if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty())
    {
      auto const Value = State.getValueAPFloat(Instruction);
      if (Value.assigned<llvm::APFloat>()) {
        llvm::SmallString<32> Buffer;
        Value.get<llvm::APFloat>().toString(Buffer);
        Out << "    " << i << " = (long double)" << Buffer << "\n";
      }
    }
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
