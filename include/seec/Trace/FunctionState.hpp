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

#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/StateCommon.hpp"
#include "seec/Util/IndexTypesForLLVMObjects.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/Range.hpp"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Instructions.h"

#include <cstdint>
#include <functional>
#include <vector>
#include <deque>


namespace llvm {
  class raw_ostream;
  class AllocaInst;
  class DataLayout;
  class Instruction;
  class Function;
}


namespace seec {

class FunctionIndex;

namespace trace {

class FunctionState;
class FunctionTrace;
class ThreadState;

namespace value_store {
  class BasicBlockStore;
  class FunctionInfo;
  class ModuleInfo;
}

/// \brief Represents the result of a single alloca instruction.
///
class AllocaState {
  /// Index of the llvm::AllocaInst.
  InstrIndexInFn m_InstructionIndex;

  /// Runtime address for this allocation.
  stateptr_ty m_Address;

  /// Total size of this allocation.
  std::size_t m_TotalSize;

public:
  /// Construct a new AllocaState with the specified values.
  AllocaState(FunctionState const &Parent,
              InstrIndexInFn InstructionIndex,
              stateptr_ty Address,
              std::size_t ElementSize,
              std::size_t ElementCount)
  : m_InstructionIndex(InstructionIndex),
    m_Address(Address),
    m_TotalSize(ElementSize * ElementCount)
  {}


  /// \name Accessors
  /// @{

  /// \brief Get the index of the llvm::AllocaInst that produced this state.
  InstrIndexInFn getInstructionIndex() const { return m_InstructionIndex; }

  /// \brief Get the runtime address for this allocation.
  stateptr_ty getAddress() const { return m_Address; }

  /// \brief Get the total size of this allocation.
  std::size_t getTotalSize() const { return m_TotalSize; }

  /// @} (Accessors)


  /// \name Queries
  /// @{

  /// \brief Get the llvm::AllocaInst that produced this state.
  llvm::AllocaInst const *getInstruction(FunctionState const &Parent) const;

  /// @} (Queries)


  /// \name Operators
  /// @{

  bool operator==(AllocaState const &RHS) const {
    return this->m_InstructionIndex == RHS.m_InstructionIndex
        && this->m_Address == RHS.m_Address
        && this->m_TotalSize == RHS.m_TotalSize;
  }

  /// @} (Operators)
};


/// \brief Information about a parameter passed byval.
///
class ParamByValState {
  /// The parameter's llvm::Argument.
  llvm::Argument const *Arg;
  
  /// The memory area occupied by the parameter.
  MemoryArea Area;
  
public:
  /// \brief Constructor.
  ///
  ParamByValState(llvm::Argument const *ForArg,
                  MemoryArea const &WithArea)
  : Arg(ForArg),
    Area(WithArea)
  {}
  
  /// \brief Get the parameter's llvm::Argument.
  ///
  llvm::Argument const *getArgument() const { return Arg; }
  
  /// \brief Get the memory area occupied by the parameter.
  ///
  MemoryArea const &getArea() const { return Area; }
};


/// \brief Represents a single RunError.
///
class RuntimeErrorState {
  /// The function state that this error belongs to.
  FunctionState const &Parent;
  
  /// The index of the Instruction that caused this error.
  InstrIndexInFn InstructionIndex;
  
  /// The runtime error.
  std::unique_ptr<seec::runtime_errors::RunError> Error;
  
  /// The thread time at which this error occurred.
  uint64_t ThreadTime;
  
public:
  /// \brief Constructor.
  ///
  RuntimeErrorState(FunctionState const &WithParent,
                    InstrIndexInFn WithInstructionIndex,
                    std::unique_ptr<seec::runtime_errors::RunError> WithError,
                    uint64_t AtThreadTime)
  : Parent(WithParent),
    InstructionIndex(WithInstructionIndex),
    Error(std::move(WithError)),
    ThreadTime(AtThreadTime)
  {}
  
  /// \brief Get the function state that this runtime error belongs to.
  ///
  FunctionState const &getParent() const { return Parent; }
  
  /// \brief Get the index of the instruction that produced this error.
  ///
  InstrIndexInFn getInstructionIndex() const { return InstructionIndex; }
  
  /// \brief Get the Instruction that produced this error.
  ///
  llvm::Instruction const *getInstruction() const;
  
  /// \brief Get the RunError itself.
  ///
  seec::runtime_errors::RunError const &getRunError() const { return *Error; }
  
  /// \brief Get the thread time at which this error occurred.
  ///
  uint64_t getThreadTime() const { return ThreadTime; }
  
  
  /// \name Queries
  /// @{
  
  /// \brief Check if this runtime error is currently active.
  ///
  bool isActive() const;
  
  /// @} (Queries.)
};


/// \brief Records BasicBlocks affected by a backwards jump.
/// This is used to correctly rewind changes to the runtime values when we
/// rewind over a backwards jump.
///
struct BasicBlockBackwardsJumpRecord {
  /// The \c llvm::BasicBlock that was jumped from.
  llvm::BasicBlock const *FromBlock;

  /// The number of BasicBlocks cleared by the jump.
  std::size_t NumCleared;
  
  /// \brief Constructor.
  /// \param From The \c llvm::BasicBlock that was jumped from.
  /// \param Cleared The number of BasicBlocks cleared by the jump.
  ///
  BasicBlockBackwardsJumpRecord(llvm::BasicBlock const *From,
                                std::size_t Cleared)
  : FromBlock(From),
    NumCleared(Cleared)
  {}
};


/// \brief State of a function invocation at a specific point in time.
///
class FunctionState {
  /// The ThreadState that this FunctionState belongs to.
  ThreadState *Parent;

  /// Indexed view of the llvm::Function.
  FunctionIndex const *FunctionLookup;
  
  /// Information by the run-time value store.
  value_store::FunctionInfo const &ValueStoreInfo;

  /// Index of the llvm::Function in the llvm::Module.
  uint32_t Index;

  /// Function trace record.
  std::unique_ptr<FunctionTrace> m_Trace;

  /// Index of the currently active llvm::Instruction.
  llvm::Optional<InstrIndexInFn> ActiveInstruction;

  /// true iff the active \llvm::Instruction has completed execution.
  bool ActiveInstructionComplete;

  /// All active stack allocations for this function.
  std::vector<AllocaState> Allocas;
  
  /// Stack allocatioins that have been cleared by stackrestore.
  std::deque<AllocaState> m_ClearedAllocas;
  
  /// All byval argument memory areas for this function.
  std::vector<ParamByValState> ParamByVals;
  
  /// All runtime errors seen in this function.
  std::vector<RuntimeErrorState> RuntimeErrors;
  
  /// Currently active BasicBlocks and the RTValues they contain.
  llvm::DenseMap<llvm::BasicBlock const *,
                 std::unique_ptr<value_store::BasicBlockStore>>
    ActiveBlocks;
  
  /// History of backwards BasicBlock jumps.
  std::vector<BasicBlockBackwardsJumpRecord> BackwardsJumps;
  
  /// Information about BasicBlocks cleared by backwards jumps.
  std::vector<std::pair<llvm::BasicBlock const *,
                        std::unique_ptr<value_store::BasicBlockStore>>>
    ClearedBlocks;

public:
  /// \brief Constructor.
  /// \param Index Index of this \c llvm::Function in the \c llvm::Module.
  /// \param Function Indexed view of the \c llvm::Function.
  /// \param ModuleStoreInfo Runtime value storage information for the
  ///        \c llvm::Module.
  /// \param Trace Trace information for this function invocation.
  FunctionState(ThreadState &Parent,
                uint32_t Index,
                FunctionIndex const &Function,
                value_store::ModuleInfo const &ModuleStoreInfo,
                std::unique_ptr<FunctionTrace> Trace);

  /// \brief Destructor.
  ///
  ~FunctionState();

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
  FunctionTrace const &getTrace() const { return *m_Trace; }

  /// \brief Get the number of llvm::Instructions in this llvm::Function.
  std::size_t getInstructionCount() const;
  
  /// \brief Get the llvm::Instruction at the specified index.
  llvm::Instruction const *getInstruction(InstrIndexInFn Index) const;

  /// \brief Get the index of the active llvm::Instruction, if there is one.
  llvm::Optional<InstrIndexInFn> getActiveInstructionIndex() const {
    return ActiveInstruction;
  }

  /// \brief Get the active llvm::Instruction, if there is one.
  llvm::Instruction const *getActiveInstruction() const;

  /// @} (Accessors)


  /// \name Mutators.
  /// @{

  /// \brief Notify that we are moving forward to the given Instruction index.
  /// \param Index index of the \c llvm::Instruction we are moving to.
  ///
  void forwardingToInstruction(InstrIndexInFn const Index);
  
  /// \brief Notify that we are moving backward to the given Instruction index.
  /// \param Index index of the \c llvm::Instruction we are moving to.
  ///
  void rewindingToInstruction(InstrIndexInFn const Index);
  
  /// \brief Set the index of the active \c llvm::Instruction and mark it as
  ///        having completed execution.
  /// \param Index the index for the new active \c llvm::Instruction.
  ///
  void setActiveInstructionComplete(InstrIndexInFn Index) {
    ActiveInstruction = Index;
    ActiveInstructionComplete = true;
  }

  /// \brief Set the index of the active \c llvm::Instruction and mark it as
  ///        having not yet completed execution.
  /// \param Index the index for the new active \c llvm::Instruction.
  ///
  void setActiveInstructionIncomplete(InstrIndexInFn Index) {
    ActiveInstruction = Index;
    ActiveInstructionComplete = false;
  }

  /// \brief Clear the currently active \c llvm::Instruction.
  ///
  void clearActiveInstruction() {
    ActiveInstruction.reset();
  }

  /// @} (Mutators)
  
  
  /// \name Access runtime values.
  /// @{

  void setValueUInt64 (llvm::Instruction const *, uint64_t);
  void setValuePtr    (llvm::Instruction const *, stateptr_ty);
  void setValueFloat  (llvm::Instruction const *, float);
  void setValueDouble (llvm::Instruction const *, double);
  void setValueAPFloat(llvm::Instruction const *, llvm::APFloat);

  bool isDominatedByActive(llvm::Instruction const *) const;
  bool hasValue(llvm::Instruction const *) const;

  llvm::Optional<int64_t>       getValueInt64  (llvm::Instruction const*) const;
  llvm::Optional<uint64_t>      getValueUInt64 (llvm::Instruction const*) const;
  llvm::Optional<stateptr_ty>   getValuePtr    (llvm::Instruction const*) const;
  llvm::Optional<float>         getValueFloat  (llvm::Instruction const*) const;
  llvm::Optional<double>        getValueDouble (llvm::Instruction const*) const;
  llvm::Optional<llvm::APFloat> getValueAPFloat(llvm::Instruction const*) const;

  /// @}


  /// \name Allocas.
  /// @{

  /// \brief Get the active stack allocations for this function.
  ///
  std::vector<AllocaState> &getAllocas() { return Allocas; }

  /// \brief Get the active stack allocations for this function.
  ///
  std::vector<AllocaState> const &getAllocas() const { return Allocas; }
  
  /// \brief Get the "visible" stack allocations for this function.
  ///
  std::vector<std::reference_wrapper<AllocaState const>>
  getVisibleAllocas() const;
  
  /// \brief Remove the top \c Num stack allocations.
  /// \return a range containing the removed allocations.
  ///
  seec::Range<decltype(m_ClearedAllocas.cbegin())> removeAllocas(size_t Num);
  
  /// \brief Unremove \c Num stack allocations.
  /// \return a range containing the restored allocations.
  ///
  seec::Range<decltype(Allocas.cbegin())> unremoveAllocas(size_t Num);

  /// @}
  
  
  /// \name Argument byval memory area tracking.
  /// @{
  
  /// \brief Get information about all parameters passed byval.
  ///
  decltype(ParamByVals) const &getParamByValStates() const {
    return ParamByVals;
  }
  
  /// \brief Get the area occupied by the given byval Arg.
  ///
  seec::Maybe<seec::MemoryArea>
  getParamByValArea(llvm::Argument const *Arg) const;
  
  /// \brief Add an argument byval memory area.
  ///
  void addByValArea(unsigned ArgumentNumber,
                    stateptr_ty Address,
                    std::size_t Size);
  
  /// \brief Remove the argument byval memory area that begins at Address.
  ///
  void removeByValArea(stateptr_ty Address);
  
  /// @} (Argument byval memory area tracking.)
  
  
  /// \name Runtime errors.
  /// @{
  
  std::vector<RuntimeErrorState> const &getRuntimeErrors() const {
    return RuntimeErrors;
  }
  
  seec::Range<decltype(RuntimeErrors)::const_iterator>
  getRuntimeErrorsActive() const;
  
  void addRuntimeError(std::unique_ptr<seec::runtime_errors::RunError> Error);
  
  void removeLastRuntimeError();
  
  /// @}
};

/// \brief Print a comparable textual description of a \c FunctionState.
///
void printComparable(llvm::raw_ostream &Out, FunctionState const &State);

/// Print a textual description of a FunctionState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_FUNCTIONSTATE_HPP
