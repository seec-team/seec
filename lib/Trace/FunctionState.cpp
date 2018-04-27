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

#include "seec/Trace/BlockValueStore.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/IsRecordableType.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/TraceReader.hpp"
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

namespace {
value_store::FunctionInfo const &
getFunctionStoreInfo(value_store::ModuleInfo const &MI, llvm::Function const &F)
{
  auto const Info = MI.getFunctionInfo(&F);
  assert(Info);
  return *Info;
}
}

FunctionState::FunctionState(ThreadState &Parent,
                             uint32_t Index,
                             FunctionIndex const &Function,
                             value_store::ModuleInfo const &ModuleStoreInfo,
                             std::unique_ptr<FunctionTrace> Trace)
: Parent(&Parent),
  FunctionLookup(&Function),
  ValueStoreInfo(getFunctionStoreInfo(ModuleStoreInfo, Function.getFunction())),
  Index(Index),
  m_Trace(std::move(Trace)),
  ActiveInstruction(),
  ActiveInstructionComplete(false),
  Allocas(),
  m_ClearedAllocas(),
  ParamByVals(),
  RuntimeErrors(),
  ActiveBlocks(),
  BackwardsJumps(),
  ClearedBlocks()
{}

FunctionState::~FunctionState() = default;

llvm::Function const *FunctionState::getFunction() const {
  return Parent->getParent().getModule().getFunction(Index);
}

std::size_t FunctionState::getInstructionCount() const {
  return FunctionLookup->getInstructionCount();
}

llvm::Instruction const *
FunctionState::getInstruction(InstrIndexInFn Index) const {
  return FunctionLookup->getInstruction(Index);
}

llvm::Instruction const *FunctionState::getActiveInstruction() const {
  if (!ActiveInstruction)
    return nullptr;

  return FunctionLookup->getInstruction(*ActiveInstruction);
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

void FunctionState::forwardingToInstruction(InstrIndexInFn const Index)
{
  auto const Current = getActiveInstruction();
  auto const I = FunctionLookup->getInstruction(Index);
  auto const IBB = I->getParent();
  
  if (Current) {
    auto const CBB = Current->getParent();
    
    // If we jump backwards, clear all BBs that we jump over,
    // including the current BB.
    if (Index < *ActiveInstruction) {
      std::size_t ClearCount = 0;
      
      for (auto BB = IBB; true; BB = BB->getNextNode()) {
        auto It = ActiveBlocks.find(BB);
        if (It != ActiveBlocks.end()) {
          ClearedBlocks.emplace_back(It->first, std::move(It->second));
          ++ClearCount;
          ActiveBlocks.erase(It);
        }
        
        if (BB == CBB) {
          break;
        }
      }
      
      BackwardsJumps.emplace_back(CBB, ClearCount);
    }
  }
  
  if (!ActiveBlocks.count(IBB)) {
    // Make the new Instruction's BasicBlock active.
    auto const BBInfo = ValueStoreInfo.getBasicBlockInfo(IBB);
    assert(BBInfo && "no basic block info for block?");
    ActiveBlocks[IBB] = llvm::make_unique<value_store::BasicBlockStore>
                                         (*BBInfo);
    assert(ActiveBlocks[IBB] && "null BasicBlockStore");
  }
}

void FunctionState::rewindingToInstruction(InstrIndexInFn const Index)
{
  auto const I = FunctionLookup->getInstruction(Index);
  auto const IBB = I->getParent();
  
  // If we jumped from a succeeding BB, unclear those that were jumped over.
  if (*ActiveInstruction < Index) {
    assert(IBB == BackwardsJumps.back().FromBlock);
    
    for (std::size_t i = 0; i < BackwardsJumps.back().NumCleared; ++i) {
      auto &Record = ClearedBlocks.back();
      ActiveBlocks[Record.first] = std::move(Record.second);
      ClearedBlocks.pop_back();
    }
    
    BackwardsJumps.pop_back();
  }
}

void FunctionState::setValueUInt64(llvm::Instruction const *ForInstruction,
                                   uint64_t const Value)
{
  auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
  auto const BB = ForInstruction->getParent();
  auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
  auto const ActiveBBIter = ActiveBlocks.find(BB);
  assert(ActiveBBIter != ActiveBlocks.end());
  auto &Store = ActiveBBIter->second;
  assert(Index && Info && Store);
  Store->setUInt64(*Info, *Index, Value);
}

void FunctionState::setValuePtr(llvm::Instruction const *ForInstruction,
                                stateptr_ty const Value)
{
  auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
  auto const BB = ForInstruction->getParent();
  auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
  auto const ActiveBBIter = ActiveBlocks.find(BB);
  assert(ActiveBBIter != ActiveBlocks.end());
  auto &Store = ActiveBBIter->second;
  assert(Index && Info && Store);
  Store->setPtr(*Info, *Index, Value);
}

void FunctionState::setValueFloat(llvm::Instruction const *ForInstruction,
                                  float const Value)
{
  auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
  auto const BB = ForInstruction->getParent();
  auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
  auto const ActiveBBIter = ActiveBlocks.find(BB);
  assert(ActiveBBIter != ActiveBlocks.end());
  auto &Store = ActiveBBIter->second;
  assert(Index && Info && Store);
  Store->setFloat(*Info, *Index, Value);
}

void FunctionState::setValueDouble(llvm::Instruction const *ForInstruction,
                                   double const Value)
{
  auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
  auto const BB = ForInstruction->getParent();
  auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
  auto const ActiveBBIter = ActiveBlocks.find(BB);
  assert(ActiveBBIter != ActiveBlocks.end());
  auto &Store = ActiveBBIter->second;
  assert(Index && Info && Store);
  Store->setDouble(*Info, *Index, Value);
}

void FunctionState::setValueAPFloat(llvm::Instruction const *ForInstruction,
                                    llvm::APFloat Value)
{
  auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
  auto const BB = ForInstruction->getParent();
  auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
  auto const ActiveBBIter = ActiveBlocks.find(BB);
  assert(ActiveBBIter != ActiveBlocks.end());
  auto &Store = ActiveBBIter->second;
  assert(Index && Info && Store);
  Store->setAPFloat(*Info, *Index, std::move(Value));
}

bool FunctionState::isDominatedByActive(llvm::Instruction const *Inst) const
{
  if (!ActiveInstruction)
    return false;

  auto const MaybeIndex = FunctionLookup->getIndexOfInstruction(Inst);
  if (!MaybeIndex)
    return false;

  if (*ActiveInstruction < *MaybeIndex)
    return false;

  if (*ActiveInstruction == *MaybeIndex)
    if (!ActiveInstructionComplete)
      return false;

  return true;
}

bool FunctionState::hasValue(llvm::Instruction const *ForInstruction) const
{
  if (!isDominatedByActive(ForInstruction))
    return false;

  auto const Type = ForInstruction->getType();
  if (!isRecordableType(Type))
    return false;

  auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
  auto const BB = ForInstruction->getParent();
  auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
  auto const ActiveBBIter = ActiveBlocks.find(BB);
  if (ActiveBBIter == ActiveBlocks.end())
    return false;
  
  auto &Store = ActiveBBIter->second;
  assert(Index && Info && Store);

  return Store->hasValue(*Info, *Index);
}

llvm::Optional<int64_t>
FunctionState::getValueInt64(llvm::Instruction const *ForInstruction) const
{
  auto const MaybeUInt64 = getValueUInt64(ForInstruction);

  if (MaybeUInt64) {
    auto const IntTy = llvm::dyn_cast<llvm::IntegerType>
                                     (ForInstruction->getType());

    auto const Value = *MaybeUInt64;

    // If the integer has its sign bit set, then set all higher bits.
    if (Value & IntTy->getSignBit()) {
      return static_cast<int64_t>(Value | ~IntTy->getBitMask());
    }

    return static_cast<int64_t>(Value);
  }

  return llvm::Optional<int64_t>();
}

llvm::Optional<uint64_t>
FunctionState::getValueUInt64(llvm::Instruction const *ForInstruction) const
{
  auto RetVal = llvm::Optional<uint64_t>();
  
  if (isDominatedByActive(ForInstruction)) {
    auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
    auto const BB = ForInstruction->getParent();
    auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
    auto const ActiveBBIter = ActiveBlocks.find(BB);
    if (ActiveBBIter != ActiveBlocks.end()) {
      auto &Store = ActiveBBIter->second;
      assert(Index && Info && Store);
      RetVal = Store->getUInt64(*Info, *Index);
    }
  }
  
  return RetVal;
}

llvm::Optional<stateptr_ty>
FunctionState::getValuePtr(llvm::Instruction const *ForInstruction) const
{
  auto RetVal = llvm::Optional<stateptr_ty>();
  
  if (isDominatedByActive(ForInstruction)) {
    auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
    auto const BB = ForInstruction->getParent();
    auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
    auto const ActiveBBIter = ActiveBlocks.find(BB);
    if (ActiveBBIter != ActiveBlocks.end()) {
      auto &Store = ActiveBBIter->second;
      assert(Index && Info && Store);
      RetVal = Store->getPtr(*Info, *Index);
    }
  }
  
  return RetVal;
}

llvm::Optional<float>
FunctionState::getValueFloat(llvm::Instruction const *ForInstruction) const
{
  auto RetVal = llvm::Optional<float>();
  
  if (isDominatedByActive(ForInstruction)) {
    auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
    auto const BB = ForInstruction->getParent();
    auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
    auto const ActiveBBIter = ActiveBlocks.find(BB);
    if (ActiveBBIter != ActiveBlocks.end()) {
      auto &Store = ActiveBBIter->second;
      assert(Index && Info && Store);
      RetVal = Store->getFloat(*Info, *Index);
    }
  }
  
  return RetVal;
}

llvm::Optional<double>
FunctionState::getValueDouble(llvm::Instruction const *ForInstruction) const
{
  auto RetVal = llvm::Optional<double>();
  
  if (isDominatedByActive(ForInstruction)) {
    auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
    auto const BB = ForInstruction->getParent();
    auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
    auto const ActiveBBIter = ActiveBlocks.find(BB);
    if (ActiveBBIter != ActiveBlocks.end()) {
      auto &Store = ActiveBBIter->second;
      assert(Index && Info && Store);
      RetVal = Store->getDouble(*Info, *Index);
    }
  }
  
  return RetVal;
}

llvm::Optional<llvm::APFloat>
FunctionState::getValueAPFloat(llvm::Instruction const *ForInstruction) const
{
  auto RetVal = llvm::Optional<llvm::APFloat>();
  
  if (isDominatedByActive(ForInstruction)) {
    auto const Index = FunctionLookup->getIndexOfInstruction(ForInstruction);
    auto const BB = ForInstruction->getParent();
    auto const Info = ValueStoreInfo.getBasicBlockInfo(BB);
    auto const ActiveBBIter = ActiveBlocks.find(BB);
    if (ActiveBBIter != ActiveBlocks.end()) {
      auto &Store = ActiveBBIter->second;
      assert(Index && Info && Store);
      
      auto Value = Store->getAPFloat(*Info, *Index);
      if (Value) {
        RetVal = *Value;
      }
    }
  }
  
  return RetVal;
}

std::vector<std::reference_wrapper<AllocaState const>>
FunctionState::getVisibleAllocas() const {
  std::vector<std::reference_wrapper<AllocaState const>> RetVal;
  
  if (!ActiveInstruction)
    return RetVal;
  
  auto const ActiveIdx = *ActiveInstruction;
  
  for (auto const &Alloca : Allocas) {
    auto const Inst = Alloca.getInstruction();
    auto const MaybeIdx = FunctionLookup->getIndexOfDbgDeclareFor(Inst);
    
    // If the index of the llvm.dbg.declare is greater than our active index,
    // then do not show this alloca. If the llvm.dbg.declare is the very next
    // instruction, then we should still show this.
    auto const NextActiveIdx = InstrIndexInFn{ActiveIdx.raw() + 1};
    if (MaybeIdx && *MaybeIdx > NextActiveIdx)
      continue;
    
    RetVal.emplace_back(Alloca);
  }
  
  return RetVal;
}

auto FunctionState::removeAllocas(size_t Num)
  -> seec::Range<decltype(m_ClearedAllocas.cbegin())>
{
  assert(Num <= Allocas.size());
  
  if (Num > 0) {
    auto const PopBegin = Allocas.end() - Num;
    auto const PopEnd   = Allocas.end();
    
    std::move(PopBegin, PopEnd, std::back_inserter(m_ClearedAllocas));
    Allocas.erase(PopBegin, PopEnd);
  }
  
  return range(m_ClearedAllocas.cend() - Num, m_ClearedAllocas.cend());
}

auto FunctionState::unremoveAllocas(size_t Num)
  -> seec::Range<decltype(Allocas.cbegin())>
{
  assert(Num <= m_ClearedAllocas.size());
  
  if (Num > 0) {
    auto const UnpopBegin = m_ClearedAllocas.end() - Num;
    auto const UnpopEnd   = m_ClearedAllocas.end();
    
    std::move(UnpopBegin, UnpopEnd, std::back_inserter(Allocas));
    m_ClearedAllocas.erase(UnpopBegin, UnpopEnd);
  }
  
  return range(Allocas.cend() - Num, Allocas.cend());
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
  assert(ActiveInstruction && "Runtime error with no active instruction.");
  
  RuntimeErrors.emplace_back(*this,
                             *ActiveInstruction,
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
    Out << "    " << Alloca.getInstructionIndex().raw()
        <<  " =[" << Alloca.getElementCount()
        <<    "x" << Alloca.getElementSize()
        <<  "]\n";
  }

  Out << "   Instruction values [Active=";
  if (State.getActiveInstructionIndex())
    Out << State.getActiveInstructionIndex()->raw();
  else
    Out << "unassigned";
  Out << "]:\n";

  auto const InstructionCount = State.getInstructionCount();
  for (std::size_t i = 0; i < InstructionCount; ++i) {
    auto const InstrIdx = InstrIndexInFn{static_cast<uint32_t>(i)};
    auto const Instruction = State.getInstruction(InstrIdx);
    auto const Type = Instruction->getType();

    if (llvm::isa<llvm::IntegerType>(Type)) {
      auto const UValue = State.getValueUInt64(Instruction);
      if (UValue) {
        auto const SValue = State.getValueInt64(Instruction);
        assert(SValue);

        Out << "    " << i << " = (int64_t)" << *SValue
                           << ", (uint64_t)" << *UValue << "\n";
      }
    }
    else if (Type->isPointerTy()) {
      auto const Value = State.getValuePtr(Instruction);
      if (Value) {
        Out << "    " << i << " = \n";
        // TODO: a comparable pointer representation (this requires us to
        //       determine the allocation that a pointer references, and then
        //       display the pointer value relative to that allocation).
      }
    }
    else if (Type->isFloatTy()) {
      auto const Value = State.getValueFloat(Instruction);
      if (Value) {
        Out << "    " << i << " = (float)" << *Value << "\n";
      }
    }
    else if (Type->isDoubleTy()) {
      auto const Value = State.getValueDouble(Instruction);
      if (Value) {
        Out << "    " << i << " = (double)" << *Value << "\n";
      }
    }
    else if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty())
    {
      auto const Value = State.getValueAPFloat(Instruction);
      if (Value) {
        llvm::SmallString<32> Buffer;
        Value->toString(Buffer);
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
    Out << "    " << Alloca.getInstructionIndex().raw()
        <<  " =[" << Alloca.getElementCount()
        <<    "x" << Alloca.getElementSize()
        <<  "] @" << Alloca.getAddress()
        << "\n";
  }

  Out << "   Instruction values [Active=";
  if (State.getActiveInstructionIndex())
    Out << State.getActiveInstructionIndex()->raw();
  else
    Out << "unassigned";
  Out << "]:\n";
  
  auto const InstructionCount = State.getInstructionCount();
  for (std::size_t i = 0; i < InstructionCount; ++i) {
    auto const InstrIdx = InstrIndexInFn{static_cast<uint32_t>(i)};
    auto const Instruction = State.getInstruction(InstrIdx);
    auto const Type = Instruction->getType();
    
    if (llvm::isa<llvm::IntegerType>(Type)) {
      if (auto const Value = State.getValueUInt64(Instruction)) {
        Out << "    " << i << " = (uint64_t)" << *Value << "\n";
      }
    }
    else if (Type->isPointerTy()) {
      if (auto const Value = State.getValuePtr(Instruction)) {
        Out << "    " << i << " = (? *)" << *Value << "\n";
      }
    }
    else if (Type->isFloatTy()) {
      if (auto const Value = State.getValueFloat(Instruction)) {
        Out << "    " << i << " = (float)" << *Value << "\n";
      }
    }
    else if (Type->isDoubleTy()) {
      if (auto const Value = State.getValueDouble(Instruction)) {
        Out << "    " << i << " = (double)" << *Value << "\n";
      }
    }
    else if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty())
    {
      if (auto const Value = State.getValueAPFloat(Instruction)) {
        llvm::SmallString<32> Buffer;
        Value->toString(Buffer);
        Out << "    " << i << " = (long double)" << Buffer << "\n";
      }
    }
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
