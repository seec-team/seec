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


namespace seec {

namespace trace {

namespace value_store {

namespace {

/// \brief Get the store size for runtime values of the given type.
/// Note that this doesn't match the program's original store size. In
/// particular, we currently store all integers as uint64_t. Pointers
/// are also stored as \c stateptr_ty.
///
llvm::Optional<int32_t> getRecreatedStoreSizeForType(llvm::Type const *Type)
{
  llvm::Optional<int32_t> RetVal;
  
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
: m_InstrIndexBase(0),
  m_InstrCount(0),
  m_LongDoubleInstrCount(0),
  m_TotalDataSize(0),
  m_IndicesAndOffsets(nullptr)
{
  if (BB.size() == 0) {
    return;
  }

  m_InstrIndexBase =
    int32_t(FnIndex.getIndexOfInstruction(&*(BB.begin())).get<uint32_t>());
  m_IndicesAndOffsets.reset(new IndexOrOffsetRecord[BB.size()]);

  for (auto const &I : range(BB.begin(), BB.end())) {
    auto const Type = I.getType();

    if (isRecordableType(Type)) {
      if (Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty()) {
        m_IndicesAndOffsets[m_InstrCount].m_IsAPFloatIndex = true;
        m_IndicesAndOffsets[m_InstrCount].m_Value = m_LongDoubleInstrCount;
        ++m_LongDoubleInstrCount;
      }
      else if (auto const Size = getRecreatedStoreSizeForType(Type)) {
        m_IndicesAndOffsets[m_InstrCount].m_IsDataOffset = true;
        m_IndicesAndOffsets[m_InstrCount].m_Value = m_TotalDataSize;
        m_TotalDataSize += *Size;
      }
      else {
        llvm_unreachable("not sure what to do with this recordable type");
      }
    }
    
    ++m_InstrCount;
  }
}

int32_t BasicBlockInfo::getInstructionIndexBase() const
{
  return m_InstrIndexBase;
}

int32_t BasicBlockInfo::getInstructionCount() const
{
  return m_InstrCount;
}

int32_t BasicBlockInfo::getLongDoubleInstructionCount() const
{
  return m_LongDoubleInstrCount;
}

int32_t BasicBlockInfo::getTotalDataSize() const
{
  return m_TotalDataSize;
}

int64_t BasicBlockInfo::getAdjustedIndex(uint32_t const InstrIndex)
const
{
  assert(int64_t(InstrIndex) >= int64_t(m_InstrIndexBase));
  auto const Index = int64_t(InstrIndex) - int64_t(m_InstrIndexBase);
  assert(Index < getInstructionCount());
  return Index;
}

llvm::Optional<int32_t>
BasicBlockInfo::getDataOffset(uint32_t const InstrIndex) const
{
  auto RetVal = llvm::Optional<int32_t>();
  auto const AdjustedInstrIndex = getAdjustedIndex(InstrIndex);
  auto const &Record = m_IndicesAndOffsets[AdjustedInstrIndex];
  if (Record.m_IsDataOffset) {
    RetVal = int32_t(Record.m_Value);
  }
  return RetVal;
}

llvm::Optional<int32_t>
BasicBlockInfo::getAPFloatIndex(uint32_t const InstrIndex) const
{
  auto RetVal = llvm::Optional<int32_t>();
  auto const AdjustedInstrIndex = getAdjustedIndex(InstrIndex);
  auto const &Record = m_IndicesAndOffsets[AdjustedInstrIndex];
  if (Record.m_IsAPFloatIndex) {
    RetVal = int32_t(Record.m_Value);
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
: m_Data(new char[Info.getTotalDataSize()]),
  m_ValuesSet(Info.getInstructionCount(), false),
  m_LongDoubles(Info.getLongDoubleInstructionCount(), llvm::APFloat(0.0f))
{}

bool
BasicBlockStore::hasValue(BasicBlockInfo const &Info,
                          uint32_t const InstrIndex)
const
{
  return m_ValuesSet[Info.getAdjustedIndex(InstrIndex)];
}

void BasicBlockStore::setHasValue(BasicBlockInfo const &Info,
                                  uint32_t const InstrIndex)
{
  m_ValuesSet[Info.getAdjustedIndex(InstrIndex)] = true;
}

void BasicBlockStore::setUInt64(BasicBlockInfo const &Info,
                                uint32_t const InstrIndex,
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
                             uint32_t const InstrIndex,
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
                               uint32_t const InstrIndex,
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
                                uint32_t const InstrIndex,
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
                                 uint32_t const InstrIndex,
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
                           uint32_t const InstrIndex,
                           std::unique_ptr<char []> const &Data)
{
  auto RetVal = llvm::Optional<T>();
  
  if (Store.hasValue(Info, InstrIndex)) {
    if (auto const Offset = Info.getDataOffset(InstrIndex)) {
      assert(int32_t(sizeof(uint64_t)) <= Info.getTotalDataSize() - *Offset);
      char * const RawData = Data.get() + *Offset;
      RetVal = *reinterpret_cast<T *>(RawData);
    }
  }
  
  return RetVal;
}
} // anonymous namespace

llvm::Optional<uint64_t>
BasicBlockStore::getUInt64(BasicBlockInfo const &Info,
                           uint32_t const InstrIndex)
const
{
  return getValue<uint64_t>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<stateptr_ty>
BasicBlockStore::getPtr(BasicBlockInfo const &Info,
                        uint32_t const InstrIndex)
const
{
  return getValue<stateptr_ty>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<float>
BasicBlockStore::getFloat(BasicBlockInfo const &Info,
                          uint32_t const InstrIndex)
const
{
  return getValue<float>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<double>
BasicBlockStore::getDouble(BasicBlockInfo const &Info,
                           uint32_t const InstrIndex)
const
{
  return getValue<double>(*this, Info, InstrIndex, m_Data);
}

llvm::Optional<llvm::APFloat>
BasicBlockStore::getAPFloat(BasicBlockInfo const &Info,
                            uint32_t const InstrIndex)
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
