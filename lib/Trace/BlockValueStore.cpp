//===- lib/Trace/BlockValueStore.cpp --------------------------------------===//
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
#include "seec/Trace/IsRecordableType.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <type_safe/narrow_cast.hpp>


namespace seec {

namespace trace {

namespace value_store {

namespace {

/// \brief Get the store size for runtime values of the given type.
/// Note that this doesn't match the program's original store size. In
/// particular, we currently store all integers as uint64_t. Pointers
/// are also stored as \c stateptr_ty.
///
llvm::Optional<type_safe::size_t>
getRecreatedStoreSizeForType(llvm::Type const *Type)
{
  llvm::Optional<type_safe::size_t> RetVal;
  
  if (auto IT = llvm::dyn_cast<llvm::IntegerType>(Type)) {
    if (IT->getBitWidth() <= 64) {
      RetVal = sizeof(uint64_t);
    }
  }
  else if (Type->isPointerTy()) {
    RetVal = sizeof(stateptr_ty);
  }
  else if (Type->isFloatTy()) {
    RetVal = sizeof(float);
  }
  else if (Type->isDoubleTy()) {
    RetVal = sizeof(double);
  }
  
  return RetVal;
}
  
} // anonymous namespace

BasicBlockInfo::BasicBlockInfo(llvm::BasicBlock const &BB,
                               FunctionIndex const &FnIndex)
: m_InstrIndexBase(0u),
  m_InstrCount(0u),
  m_LongDoubleInstrCount(0u),
  m_TotalDataSize(0u),
  m_IndicesAndOffsets(nullptr)
{
  if (BB.size() == 0) {
    return;
  }

  m_InstrIndexBase = *FnIndex.getIndexOfInstruction(&*(BB.begin()));

  m_IndicesAndOffsets.reset(new IndexOrOffsetRecord[BB.size()]);

  for (auto const &I : range(BB.begin(), BB.end())) {
    auto const Type = I.getType();

    if (isRecordableType(Type)) {
      auto const RawIndex = static_cast<uint32_t>(m_InstrCount);
      
      if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty()) {
        m_IndicesAndOffsets[RawIndex].m_IsAPFloatIndex = true;
        m_IndicesAndOffsets[RawIndex].m_Value =
          static_cast<uint32_t>(m_LongDoubleInstrCount);
        ++m_LongDoubleInstrCount;
      }
      else if (auto const Size = getRecreatedStoreSizeForType(Type)) {
        m_IndicesAndOffsets[RawIndex].m_IsDataOffset = true;
        m_IndicesAndOffsets[RawIndex].m_Value =
          static_cast<uint32_t>(m_TotalDataSize);
        
        m_TotalDataSize += type_safe::narrow_cast<type_safe::uint32_t>(*Size);
      }
      else {
        llvm_unreachable("not sure what to do with this recordable type");
      }
    }
    
    ++m_InstrCount;
  }
}

InstrIndexInFn BasicBlockInfo::getInstructionIndexBase() const
{
  return m_InstrIndexBase;
}

type_safe::uint32_t BasicBlockInfo::getInstructionCount() const
{
  return m_InstrCount;
}

type_safe::uint32_t BasicBlockInfo::getLongDoubleInstructionCount() const
{
  return m_LongDoubleInstrCount;
}

type_safe::uint32_t BasicBlockInfo::getTotalDataSize() const
{
  return m_TotalDataSize;
}

InstrIndexInBB BasicBlockInfo::getAdjustedIndex(InstrIndexInFn const InstrIndex)
const
{
  assert(InstrIndex >= m_InstrIndexBase && "Instruction not in BasicBlock");
  auto const Index = static_cast<uint32_t>(InstrIndex)
                   - static_cast<uint32_t>(m_InstrIndexBase);
  assert(Index < getInstructionCount() && "Instruction not in BasicBlock");
  return InstrIndexInBB{Index};
}

llvm::Optional<uint32_t>
BasicBlockInfo::getDataOffset(InstrIndexInFn const InstrIndex) const
{
  auto RetVal = llvm::Optional<uint32_t>();
  auto const BBIndex = getAdjustedIndex(InstrIndex);
  auto const BBIndexRaw = static_cast<uint32_t>(BBIndex);
  auto const &Record = m_IndicesAndOffsets[BBIndexRaw];
  if (Record.m_IsDataOffset) {
    RetVal = uint32_t(Record.m_Value);
  }
  return RetVal;
}

llvm::Optional<uint32_t>
BasicBlockInfo::getAPFloatIndex(InstrIndexInFn const InstrIndex) const
{
  auto RetVal = llvm::Optional<uint32_t>();
  auto const BBIndex = getAdjustedIndex(InstrIndex);
  auto const BBIndexRaw = static_cast<uint32_t>(BBIndex);
  auto const &Record = m_IndicesAndOffsets[BBIndexRaw];
  if (Record.m_IsAPFloatIndex) {
    RetVal = uint32_t(Record.m_Value);
  }
  return RetVal;
}

FunctionInfo::FunctionInfo(llvm::Function const &ForFunction,
                           FunctionIndex const &WithFunctionIndex)
: m_BasicBlockInfoMap()
{
  for (auto &BB : ForFunction) {
    m_BasicBlockInfoMap.insert(
      std::make_pair(&BB,
                     llvm::make_unique<BasicBlockInfo>(BB,
                                                       WithFunctionIndex)));
  }
}

BasicBlockInfo const *
FunctionInfo::getBasicBlockInfo(llvm::BasicBlock const *BB)
const
{
  auto It = m_BasicBlockInfoMap.find(BB);
  return It != m_BasicBlockInfoMap.end() ? It->second.get() : nullptr;
}

ModuleInfo::ModuleInfo(llvm::Module const &ForModule,
                       ModuleIndex const &WithModuleIndex)
: m_FunctionInfoMap()
{
  for (auto &F : ForModule) {
    if (!F.isDeclaration()) {
      auto const FnIndex = WithModuleIndex.getFunctionIndex(&F);
      assert(FnIndex);
      
      m_FunctionInfoMap.insert(
        std::make_pair(&F,
                       llvm::make_unique<FunctionInfo>(F, *FnIndex)));
    }
  }
}

FunctionInfo const *ModuleInfo::getFunctionInfo(llvm::Function const *F)
const
{
  auto It = m_FunctionInfoMap.find(F);
  return It != m_FunctionInfoMap.end() ? It->second.get() : nullptr;
}

BasicBlockStore::BasicBlockStore(BasicBlockInfo const &Info)
: m_Data(new char[static_cast<uint32_t>(Info.getTotalDataSize())]),
  m_ValuesSet(static_cast<uint32_t>(Info.getInstructionCount()), false),
  m_LongDoubles(static_cast<uint32_t>(Info.getLongDoubleInstructionCount()),
                llvm::APFloat(0.0f))
{}

bool
BasicBlockStore::hasValue(BasicBlockInfo const &Info,
                          InstrIndexInFn const InstrIndex)
const
{
  auto const IndexInBB = Info.getAdjustedIndex(InstrIndex);
  return m_ValuesSet[static_cast<uint32_t>(IndexInBB)];
}

void BasicBlockStore::setHasValue(BasicBlockInfo const &Info,
                                  InstrIndexInFn const InstrIndex)
{
  auto const IndexInBB = Info.getAdjustedIndex(InstrIndex);
  m_ValuesSet[static_cast<uint32_t>(IndexInBB)] = true;
}

void BasicBlockStore::setUInt64(BasicBlockInfo const &Info,
                                InstrIndexInFn const InstrIndex,
                                uint64_t const Value)
{
  if (auto const Offset = Info.getDataOffset(InstrIndex)) {
    assert(!hasValue(Info, InstrIndex) && "value already exists");
    char * const RawData = m_Data.get() + *Offset;
    *reinterpret_cast<uint64_t *>(RawData) = Value;
    setHasValue(Info, InstrIndex);
  }
}

void BasicBlockStore::setPtr(BasicBlockInfo const &Info,
                             InstrIndexInFn const InstrIndex,
                             stateptr_ty const Value)
{
  if (auto const Offset = Info.getDataOffset(InstrIndex)) {
    assert(!hasValue(Info, InstrIndex) && "value already exists");
    char * const RawData = m_Data.get() + *Offset;
    *reinterpret_cast<stateptr_ty *>(RawData) = Value;
    setHasValue(Info, InstrIndex);
  }
}

void BasicBlockStore::setFloat(BasicBlockInfo const &Info,
                               InstrIndexInFn const InstrIndex,
                               float const Value)
{
  if (auto const Offset = Info.getDataOffset(InstrIndex)) {
    assert(!hasValue(Info, InstrIndex) && "value already exists");
    char * const RawData = m_Data.get() + *Offset;
    *reinterpret_cast<float *>(RawData) = Value;
    setHasValue(Info, InstrIndex);
  }
}

void BasicBlockStore::setDouble(BasicBlockInfo const &Info,
                                InstrIndexInFn const InstrIndex,
                                double const Value)
{
  if (auto const Offset = Info.getDataOffset(InstrIndex)) {
    assert(!hasValue(Info, InstrIndex) && "value already exists");
    char * const RawData = m_Data.get() + *Offset;
    *reinterpret_cast<double *>(RawData) = Value;
    setHasValue(Info, InstrIndex);
  }
}

void BasicBlockStore::setAPFloat(BasicBlockInfo const &Info,
                                 InstrIndexInFn const InstrIndex,
                                 llvm::APFloat Value)
{
  if (auto const Index = Info.getAPFloatIndex(InstrIndex)) {
    m_LongDoubles[*Index] = std::move(Value);
    setHasValue(Info, InstrIndex);
  }
}

namespace {
template<typename T>
llvm::Optional<T> getValue(BasicBlockStore const &Store,
                           BasicBlockInfo const &Info,
                           InstrIndexInFn const InstrIndex,
                           std::unique_ptr<char []> const &Data)
{
  auto RetVal = llvm::Optional<T>();
  
  if (Store.hasValue(Info, InstrIndex)) {
    if (auto const Offset = Info.getDataOffset(InstrIndex)) {
      auto const AvailableBytes = Info.getTotalDataSize() - *Offset;
      assert(AvailableBytes >= sizeof(T) && "value exceeds store size?");

      char * const RawData = Data.get() + *Offset;
      RetVal = *reinterpret_cast<T *>(RawData);
    }
  }
  
  return RetVal;
}
} // anonymous namespace

llvm::Optional<uint64_t>
BasicBlockStore::getUInt64(BasicBlockInfo const &Info,
                           InstrIndexInFn const InstrIndex)
const
{
  return getValue<uint64_t>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<stateptr_ty>
BasicBlockStore::getPtr(BasicBlockInfo const &Info,
                        InstrIndexInFn const InstrIndex)
const
{
  return getValue<stateptr_ty>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<float>
BasicBlockStore::getFloat(BasicBlockInfo const &Info,
                          InstrIndexInFn const InstrIndex)
const
{
  return getValue<float>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<double>
BasicBlockStore::getDouble(BasicBlockInfo const &Info,
                           InstrIndexInFn const InstrIndex)
const
{
  return getValue<double>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<llvm::APFloat>
BasicBlockStore::getAPFloat(BasicBlockInfo const &Info,
                            InstrIndexInFn const InstrIndex)
const
{
  auto RetVal = llvm::Optional<llvm::APFloat>();
  
  if (hasValue(Info, InstrIndex)) {
    if (auto const Index = Info.getAPFloatIndex(InstrIndex)) {
      RetVal = m_LongDoubles[*Index];
    }
  }
  
  return RetVal;
}

} // value_store

} // namespace trace (in seec)

} // namespace seec
