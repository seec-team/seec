//===- include/seec/Trace/FunctionState.hpp ------------------------- C++ -===//
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

#ifndef SEEC_TRACE_FUNCTIONSTATE_HPP
#define SEEC_TRACE_FUNCTIONSTATE_HPP

#include "seec/DSA/MemoryBlock.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/RuntimeValue.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/IR/Instructions.h"

#include <cstdint>
#include <vector>


namespace llvm {
  class raw_ostream;
  class AllocaInst;
  class Instruction;
  class Function;
}


namespace seec {

class FunctionIndex;

namespace trace {

class FunctionState;
class ThreadState;


/// \brief Represents the result of a single alloca instruction.
///
class AllocaState {
  /// The FunctionState that this AllocaState belongs to.
  FunctionState const *Parent;

  /// Index of the llvm::AllocaInst.
  uint32_t InstructionIndex;

  /// Runtime address for this allocation.
  uintptr_t Address;

  /// Size of the element type that this allocation was for.
  std::size_t ElementSize;

  /// Number of elements that space was allocated for.
  std::size_t ElementCount;

public:
  /// Construct a new AllocaState with the specified values.
  AllocaState(FunctionState const &Parent,
              uint32_t InstructionIndex,
              uintptr_t Address,
              std::size_t ElementSize,
              std::size_t ElementCount)
  : Parent(&Parent),
    InstructionIndex(InstructionIndex),
    Address(Address),
    ElementSize(ElementSize),
    ElementCount(ElementCount)
  {}


  /// \name Accessors
  /// @{

  /// \brief Get the FunctionState that this AllocaState belongs to.
  FunctionState const &getParent() const { return *Parent; }

  /// \brief Get the index of the llvm::AllocaInst that produced this state.
  uint32_t getInstructionIndex() const { return InstructionIndex; }

  /// \brief Get the runtime address for this allocation.
  uintptr_t getAddress() const { return Address; }

  /// \brief Get the size of the element type that this allocation was for.
  std::size_t getElementSize() const { return ElementSize; }

  /// \brief Get the number of elements that space was allocated for.
  std::size_t getElementCount() const { return ElementCount; }

  /// \brief Get the total size of this allocation.
  std::size_t getTotalSize() const { return ElementSize * ElementCount; }

  /// @} (Accessors)


  /// \name Queries
  /// @{

  /// \brief Get the llvm::AllocaInst that produced this state.
  llvm::AllocaInst const *getInstruction() const;

  /// \brief Get the region of memory belonging to this AllocaState.
  MemoryState::Region getMemoryRegion() const;

  /// @} (Queries)
};


/// \brief State of a function invocation at a specific point in time.
///
class FunctionState {
  /// The ThreadState that this FunctionState belongs to.
  ThreadState *Parent;

  /// Indexed view of the llvm::Function.
  FunctionIndex const *FunctionLookup;

  /// Index of the llvm::Function in the llvm::Module.
  uint32_t Index;

  /// Function trace record.
  FunctionTrace Trace;

  /// Index of the currently active llvm::Instruction.
  seec::util::Maybe<uint32_t> ActiveInstruction;

  /// Runtime values indexed by Instruction index.
  std::vector<RuntimeValue> InstructionValues;

  /// All active stack allocations for this function.
  std::vector<AllocaState> Allocas;

public:
  /// \brief Constructor.
  /// \param Index Index of this llvm::Function in the llvm::Module.
  /// \param Function Indexed view of the llvm::Function.
  FunctionState(ThreadState &Parent,
                uint32_t Index,
                FunctionIndex const &Function,
                FunctionTrace Trace);


  /// \name Accessors.
  /// @{

  /// \brief Get the ThreadState that this FunctionState belongs to.
  ThreadState &getParent() { return *Parent; }

  /// \brief Get the ThreadState that this FunctionState belongs to.
  ThreadState const &getParent() const { return *Parent; }

  /// \brief Get the FunctionIndex for this llvm::Function.
  FunctionIndex const &getFunctionLookup() const {
    return *FunctionLookup;
  }

  /// \brief Get the index of the llvm::Function in the llvm::Module.
  uint32_t getIndex() const { return Index; }

  /// \brief Get the llvm::Function.
  llvm::Function const *getFunction() const;

  /// \brief Get the function trace record for this function invocation.
  FunctionTrace getTrace() const { return Trace; }

  /// \brief Get the number of llvm::Instructions in this llvm::Function.
  std::size_t getInstructionCount() const { return InstructionValues.size(); }
  
  /// \brief Get the llvm::Instruction at the specified index.
  llvm::Instruction const *getInstruction(uint32_t Index) const;

  /// \brief Get the index of the active llvm::Instruction, if there is one.
  seec::util::Maybe<uint32_t> getActiveInstructionIndex() const {
    return ActiveInstruction;
  }

  /// \brief Get the active llvm::Instruction, if there is one.
  llvm::Instruction const *getActiveInstruction() const;

  /// @} (Accessors)


  /// \name Mutators.
  /// @{

  /// \brief Set the index of the active llvm::Instruction.
  /// \param Index the index for the new active llvm::Instruction.
  void setActiveInstruction(uint32_t Index) {
    ActiveInstruction = Index;
  }

  /// \brief Clear the currently active llvm::Instruction.
  void clearActiveInstruction() {
    ActiveInstruction.reset();
  }

  /// @} (Mutators)
  
  
  /// \name Support getCurrentRuntimeValueAs().
  /// @{
  
  /// \brief Get the run-time address of a Function.
  ///
  uintptr_t getRuntimeAddress(llvm::Function const *F) const;
  
  /// \brief Get the run-time address of a GlobalVariable.
  ///
  uintptr_t getRuntimeAddress(llvm::GlobalVariable const *GV) const;
  
  /// \brief Get a pointer to an Instruction's RuntimeValue, by index.
  ///
  RuntimeValue const *getCurrentRuntimeValue(uint32_t Index) const {
    assert(Index < InstructionValues.size());
    
    // If we have jumped to a prior Instruction, we consider the latter
    // Instruction values to no longer exist.
    if (ActiveInstruction.assigned(0) && ActiveInstruction.get<0>() < Index)
      return nullptr;
    
    return &InstructionValues[Index];
  }
  
  /// \brief Get a pointer to an Instruction's RuntimeValue.
  ///
  RuntimeValue const *getCurrentRuntimeValue(llvm::Instruction const *I) const;
    
  /// @}
  
  
  /// \name Access runtime values.
  /// @{
  
  /// \brief Get a reference to an Instruction's RuntimeValue, by index.
  ///
  RuntimeValue &getRuntimeValue(uint32_t Index) {
    assert(Index < InstructionValues.size());
    return InstructionValues[Index];
  }
  
  /// @}


  /// \name Allocas.
  /// @{

  /// \brief Get the active stack allocations for this function.
  ///
  decltype(Allocas) &getAllocas() { return Allocas; }

  /// \brief Get the active stack allocations for this function.
  ///
  decltype(Allocas) const &getAllocas() const { return Allocas; }
  
  /// \brief Find the Alloca that covers the given address, or nullptr if none
  ///        exists.
  ///
  AllocaState const *getAllocaContaining(uintptr_t Address) const {
    for (auto const &Alloca : Allocas) {
      auto const Area = MemoryArea(Alloca.getAddress(), Alloca.getTotalSize());
      if (Area.contains(Address))
        return &Alloca;
    }
    
    return nullptr;
  }

  /// @}
};

/// Print a textual description of a FunctionState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_FUNCTIONSTATE_HPP
