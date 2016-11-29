//===- include/seec/Trace/BlockValueStore.hpp ----------------------- C++ -===//
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

#include "seec/Trace/StateCommon.hpp"
#include "seec/Util/IndexTypesForLLVMObjects.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"

#include <type_safe/strong_typedef.hpp>
#include <type_safe/types.hpp>

#include <memory>

namespace llvm {
  class BasicBlock;
  class Function;
  class Instruction;
  class Module;
}

namespace seec {

class FunctionIndex;
class ModuleIndex;

struct InstrIndexInBB
: type_safe::strong_typedef<InstrIndexInBB, uint32_t>
{
  using strong_typedef::strong_typedef;
  
  uint32_t raw() const { return static_cast<uint32_t>(*this); }
};

namespace trace {

namespace value_store {

/// \brief Information used by \c BasicBlockStore s for a \c llvm::BasicBlock.
/// This information is shared by all \c BasicBlockStore s for a particular
/// \c llvm::BasicBlock .
///
class BasicBlockInfo {
  /// \brief Stores either the index of an APFloat or the offset of a runtime
  ///        value's raw data.
  ///
  struct IndexOrOffsetRecord {
    /// true iff this stores an APFloat index.
    bool m_IsAPFloatIndex : 1;
    
    /// true iff this stores a runtime value's data offset.
    bool m_IsDataOffset : 1;
    
    /// the stored value.
    uint32_t m_Value : 30;
  };

  /// The function-level-index of the first \c llvm::Instruction in this
  /// \c llvm::BasicBlock .
  InstrIndexInFn m_InstrIndexBase;
  
  /// The number of \c llvm::Instruction s in this \c llvm::BasicBlock.
  type_safe::uint32_t m_InstrCount;
  
  /// The number of \c llvm::Instruction s with long double types in this
  /// \c llvm::BasicBlock .
  type_safe::uint32_t m_LongDoubleInstrCount;
  
  /// The total number of bytes used to store the runtime values of all
  /// non-long-double type \c llvm::Instruction s in this \c llvm::BasicBlock.
  type_safe::uint32_t m_TotalDataSize;
  
  /// The APFloat index or raw data offset for every \c llvm::Instruction in
  /// this \c llvm::BasicBlock .
  std::unique_ptr<IndexOrOffsetRecord []> m_IndicesAndOffsets;
  
public:
  /// \brief Construct new BasicBlockInfo for the given \c llvm::BasicBlock .
  /// \param ForBasicBlock the BasicBlock.
  /// \param WithFunctionIndex the index for the BasicBlock's containing
  ///        \c llvm::Function .
  ///
  BasicBlockInfo(llvm::BasicBlock const &ForBasicBlock,
                 FunctionIndex const &WithFunctionIndex);
  
  /// \brief Get base instruction index.
  ///
  InstrIndexInFn getInstructionIndexBase() const;
  
  /// \brief Get number of instructions.
  ///
  type_safe::uint32_t getInstructionCount() const;
  
  /// \brief Get number of long double instructions.
  ///
  type_safe::uint32_t getLongDoubleInstructionCount() const;
  
  /// \brief Get total size of all (non-long-double) instruction data.
  ///
  type_safe::uint32_t getTotalDataSize() const;
  
  /// \brief Convert Function-level index to BasicBlock-level index.
  /// e.g. if the first Instruction in this BasicBlock has function-level-index
  /// of 10, and the passed \c InstrIndex is 12, this method returns 2.
  /// This will assert if InstrIndex is outside of the valid range.
  ///
  InstrIndexInBB getAdjustedIndex(InstrIndexInFn const InstrIndex) const;
  
  /// \brief Get offset of a particular instruction's data (if any).
  ///
  llvm::Optional<uint32_t> getDataOffset(InstrIndexInFn const Instr) const;
  
  /// \brief Get index of a long double instruction's APFloat (if any).
  ///
  llvm::Optional<uint32_t> getAPFloatIndex(InstrIndexInFn const Instr) const;
};

/// \brief Holds \c BasicBlockInfo for each \c llvm::BasicBlock in an
///        \c llvm::Function .
///
class FunctionInfo {
  /// Hold all \c BasicBlockInfo s for the \c llvm::Function in a map, using the
  /// \c llvm::BasicBlock pointer as the key.
  llvm::DenseMap<llvm::BasicBlock const *, std::unique_ptr<BasicBlockInfo>>
  m_BasicBlockInfoMap;
  
public:
  /// \brief Create and store \c BasicBlockInfo for every \c llvm::BasicBlock
  ///        in a \c llvm::Function.
  /// \param ForFunction the Function.
  /// \param WithFunctionIndex indexed view of the Function.
  ///
  FunctionInfo(llvm::Function const &ForFunction,
               FunctionIndex const &WithFunctionIndex);
  
  /// \brief Get the \c BasicBlockInfo for a particular \c llvm::BasicBlock in
  ///        this \c llvm::Function , if it exists.
  ///
  BasicBlockInfo const *getBasicBlockInfo(llvm::BasicBlock const *BB) const;
};

/// \brief Holds \c BasicBlockInfo for every \c llvm::BasicBlock in a given
///        \c llvm::Module .
///
class ModuleInfo {
  /// Holds \c FunctionInfo for every \c llvm::Function in the \c llvm::Module .
  llvm::DenseMap<llvm::Function const *, std::unique_ptr<FunctionInfo>>
  m_FunctionInfoMap;
  
public:
  /// \brief Create and store \c BasicBlockInfo for every \c llvm::BasicBlock
  ///        in the given \c llvm::Module .
  ///
  ModuleInfo(llvm::Module const &ForModule,
             ModuleIndex const &WithModuleIndex);
  
  /// \brief Get the \c FunctionInfo for a given \c llvm::Function in this
  ///        \c llvm::Module (if it exists).
  ///
  FunctionInfo const *getFunctionInfo(llvm::Function const *F) const;
};

/// \brief Stores run-time values for a single BasicBlock.
///
class BasicBlockStore {
  /// Raw data used to store each \c llvm::Instruction 's runtime value.
  std::unique_ptr<char []> m_Data;
  
  /// Records whether each Instruction's value is set.
  std::vector<bool> m_ValuesSet;
  
  /// Stores long double runtime values as \c llvm::APFloat objects.
  std::vector<llvm::APFloat> m_LongDoubles;
  
  /// \brief Set the \c llvm::Instruction to have a runtime value from now on.
  ///
  void setHasValue(BasicBlockInfo const &Info, InstrIndexInFn const InstrIndex);
  
public:
  /// \brief Constructor.
  ///
  BasicBlockStore(BasicBlockInfo const &Info);

  /// \brief Check if the given \c Instruction has a runtime value.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  ///
  bool hasValue(BasicBlockInfo const &Info, InstrIndexInFn const Instr) const;

  /// \brief Set the runtime value of a uint64_t type \c Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  /// \param Value the runtime value.
  ///
  void setUInt64(BasicBlockInfo const &Info,
                 InstrIndexInFn const Instr,
                 uint64_t const Value);
  
  /// \brief Set the runtime value of a pointer type \c Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  /// \param Value the runtime value.
  ///
  void setPtr(BasicBlockInfo const &Info,
              InstrIndexInFn const Instr,
              stateptr_ty const Value);
  
  /// \brief Set the runtime value of a float type \c Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  /// \param Value the runtime value.
  ///
  void setFloat(BasicBlockInfo const &Info,
                InstrIndexInFn const Instr,
                float const Value);
  
  /// \brief Set the runtime value of a double type \c Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  /// \param Value the runtime value.
  ///
  void setDouble(BasicBlockInfo const &Info,
                 InstrIndexInFn const Instr,
                 double);
  
  /// \brief Set the runtime value of a long double type \c Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  /// \param Value the runtime value.
  ///
  void setAPFloat(BasicBlockInfo const &Info,
                  InstrIndexInFn const Instr,
                  llvm::APFloat Value);

  /// \brief Get the runtime value of a uint64_t type Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  ///
  llvm::Optional<uint64_t> getUInt64(BasicBlockInfo const &Info,
                                     InstrIndexInFn const Instr) const;
  
  /// \brief Get the runtime value of a pointer type Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  ///
  llvm::Optional<stateptr_ty> getPtr(BasicBlockInfo const &Info,
                                     InstrIndexInFn const Instr) const;
  
  /// \brief Get the runtime value of a float type Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  ///
  llvm::Optional<float> getFloat(BasicBlockInfo const &Info,
                                 InstrIndexInFn const Instr) const;
  
  /// \brief Get the runtime value of a double type Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  ///
  llvm::Optional<double> getDouble(BasicBlockInfo const &Info,
                                   InstrIndexInFn const Instr) const;
  
  /// \brief Get the runtime value of a long double type Instruction.
  /// \param Info the \c BasicBlockInfo for this \c BasicBlock.
  /// \param InstrIndex the function-level-index of the \c Instruction.
  ///
  llvm::Optional<llvm::APFloat> getAPFloat(BasicBlockInfo const &Info,
                                           InstrIndexInFn const Instr) const;
};

} // value_store

} // namespace trace (in seec)

} // namespace seec
