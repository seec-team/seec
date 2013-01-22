//===- include/seec/Trace/TracedFunction.hpp ------------------------ C++ -===//
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

#ifndef SEEC_TRACE_TRACEDFUNCTION_HPP
#define SEEC_TRACE_TRACEDFUNCTION_HPP

#include "seec/DSA/MemoryArea.hpp"
#include "seec/Trace/RuntimeValue.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace llvm {

class AllocaInst;
class CallInst;
class Instruction;

} // namespace llvm

namespace seec {

namespace trace {

class TraceThreadListener;


/// \brief Stores information about a single recorded alloca instruction.
///
///
class TracedAlloca {
  /// The alloca instruction.
  llvm::AllocaInst const *Instruction;
  
  /// The address of the allocated memory.
  uintptr_t Address;
  
  /// The size of the allocated type.
  std::size_t ElementSize;
  
  /// The number of elements allocated.
  std::size_t ElementCount;
  
  /// Offset of the Alloca event.
  offset_uint EventOffset;

public:
  /// Constructor.
  TracedAlloca(llvm::AllocaInst const *Instruction,
               uintptr_t Address,
               std::size_t ElementSize,
               std::size_t ElementCount,
               offset_uint EventOffset)
  : Instruction(Instruction),
    Address(Address),
    ElementSize(ElementSize),
    ElementCount(ElementCount),
    EventOffset(EventOffset)
  {}
  
  /// Copy constructor.
  TracedAlloca(TracedAlloca const &) = default;
  
  /// Copy assignment.
  TracedAlloca &operator=(TracedAlloca const &) = default;
  
  
  /// \name Accessors
  /// @{
  
  /// Get the alloca instruction responsible for this allocation.
  llvm::AllocaInst const *instruction() const { return Instruction; }
  
  /// Get the address of the allocated memory.
  uintptr_t address() const { return Address; }
  
  /// Get the size of the allocated type.
  std::size_t elementSize() const { return ElementSize; }
  
  /// Get the number of elements allocated.
  std::size_t elementCount() const { return ElementCount; }
  
  /// Get the offset of the Alloca event.
  offset_uint eventOffset() const { return EventOffset; }
  
  /// Get the memory area occupied by this alloca.
  MemoryArea area() const {
    return MemoryArea(Address, ElementSize * ElementCount);
  }
  
  /// @}
  
  
  /// \name Operators
  /// @{
  
  bool operator==(TracedAlloca const &RHS) const {
    return Instruction == RHS.Instruction
        && Address == RHS.Address
        && ElementSize == RHS.ElementSize
        && ElementCount == RHS.ElementCount;
  }
  
  bool operator!=(TracedAlloca const &RHS) const {
    return !(*this == RHS);
  }
  
  /// @}
};


/// \brief Stores information about a single recorded Function execution.
///
///
class TracedFunction {
  // Don't allow copying.
  TracedFunction(TracedFunction const &) = delete;
  TracedFunction &operator=(TracedFunction const &) = delete;
  
  
  /// \name Permanent information.
  /// @{
  
  /// The thread that this function belongs to.
  TraceThreadListener const &ThreadListener;

  /// Indexed view of the Function.
  FunctionIndex &FIndex;

  /// Offset of the FunctionRecord for this function trace.
  offset_uint RecordOffset;

  /// Index of the Function in the LLVM Module.
  uint32_t Index;

  /// Offset of the FunctionStart event for this function trace.
  offset_uint EventOffsetStart;

  /// Offset of the FunctionEnd event for this function trace.
  offset_uint EventOffsetEnd;

  /// Thread time at which this function was entered.
  uint64_t ThreadTimeEntered;

  /// Thread time at which this function was exited.
  uint64_t ThreadTimeExited;

  /// List of offsets of FunctionRecords for the direct children of this
  /// function trace.
  std::vector<offset_uint> Children;
  
  /// @}
  
  
  /// \name Active-only information.
  /// @{
  
  /// Currently-active Instruction.
  llvm::Instruction const *ActiveInstruction;
  
  /// List of Allocas for this function.
  std::vector<TracedAlloca> Allocas;
  
  /// Areas occupied by byval arguments for this function.
  std::vector<MemoryArea> ByValAreas;
  
  /// Stores stacksaved Allocas.
  llvm::DenseMap<uintptr_t, std::vector<TracedAlloca>> StackSaves;
  
  /// Lowest address occupied by this function's stack allocated variables.
  uintptr_t StackLow;
  
  /// Highest address occupied by this function's stack allocated variables.
  uintptr_t StackHigh;
  
  /// Controls access to all stack-related information (Allocas, StackSaves,
  /// StackLow, StackHigh).
  mutable std::mutex StackMutex;
  
  /// Current runtime values of instructions.
  std::vector<RuntimeValue> CurrentValues;
  
  /// @}
  

public:
  /// Constructor.
  TracedFunction(TraceThreadListener const &ThreadListener,
                 FunctionIndex &FIndex,
                 offset_uint RecordOffset,
                 uint32_t Index,
                 offset_uint EventOffsetStart,
                 uint64_t ThreadTimeEntered)
  : ThreadListener(ThreadListener),
    FIndex(FIndex),
    RecordOffset(RecordOffset),
    Index(Index),
    EventOffsetStart(EventOffsetStart),
    EventOffsetEnd(0),
    ThreadTimeEntered(ThreadTimeEntered),
    ThreadTimeExited(0),
    Children(),
    ActiveInstruction(nullptr),
    Allocas(),
    ByValAreas(),
    StackSaves(),
    StackLow(0),
    StackHigh(0),
    CurrentValues(FIndex.getInstructionCount())
  {}


  /// \name Accessors for permanent information.
  /// @{

  /// Get FunctionIndex for the recorded Function.
  FunctionIndex const &getFunctionIndex() const { return FIndex; }

  /// Get the offset of this FunctionRecord in the thread trace.
  offset_uint getRecordOffset() const { return RecordOffset; }

  /// Get the index of the Function in the Module.
  uint32_t getIndex() const { return Index; }

  /// Get the offset of the FunctionStart record in the thread's event trace.
  offset_uint getEventOffsetStart() const { return EventOffsetStart; }

  /// Get the offset of the FunctionEnd record in the thread's event trace.
  offset_uint getEventOffsetEnd() const { return EventOffsetEnd; }

  /// Get the thread time at which this Function started recording.
  uint64_t getThreadTimeEntered() const { return ThreadTimeEntered; }

  /// Get the thread time at which this Function finished recording.
  uint64_t getThreadTimeExited() const { return ThreadTimeExited; }

  /// Get the offsets of the child FunctionRecords.
  std::vector<offset_uint> const &getChildren() const { return Children; }

  /// @} (Accessors for permanent information.)
  
  
  /// \name Support getCurrentRuntimeValue.
  /// @{
  
  /// Get the run-time address of a GlobalVariable.
  /// \param GV the GlobalVariable.
  /// \return the run-time address of GV, or 0 if it is not known.
  uintptr_t getRuntimeAddress(llvm::GlobalVariable const *GV) const;

  /// Get the run-time address of a Function.
  /// \param F the Function.
  /// \return the run-time address of F, or 0 if it is not known.
  uintptr_t getRuntimeAddress(llvm::Function const *F) const;
  
  /// @} (Support getCurrentRuntimeValue.)
  
  
  /// \name Active llvm::Instruction tracking.
  /// @{
  
  /// \brief Get the currently active llvm::Instruction, or nullptr if there
  ///        is none.
  ///
  llvm::Instruction const *getActiveInstruction() const {
    return ActiveInstruction;
  }
  
  /// \brief Set the currently active llvm::Instruction.
  ///
  void
  setActiveInstruction(llvm::Instruction const * const NewActiveInstruction) {
    ActiveInstruction = NewActiveInstruction;
  }
  
  /// \brief Clear the currently active llvm::Instruction.
  ///
  void clearActiveInstruction() {
    ActiveInstruction = nullptr;
  }
  
  /// @} (Active llvm::Instruction tracking.)
  
  
  /// \name Accessors for active-only information.
  /// @{
  
  /// Get all currently active allocas.
  decltype(Allocas) const &getAllocas() const { return Allocas; }
  
  /// Get the memory area occupied by this function's stack-allocated variables.
  /// This method is thread safe.
  MemoryArea getStackArea() const {
    std::lock_guard<std::mutex> Lock(StackMutex);
    
    assert(!EventOffsetEnd && "Function has finished recording!");
    
    return MemoryArea(StackLow, (StackHigh - StackLow) + 1);
  }
  
  /// Get the stack-allocated area that contains an address. This method is
  /// thread safe.
  seec::util::Maybe<MemoryArea>
  getContainingMemoryArea(uintptr_t Address) const;
  
  /// Get a reference to the current RuntimeValue for an Instruction.
  /// \param Idx the index of the Instruction in the Function.
  /// \return a reference to the RuntimeValue for the Instruction at Idx.
  RuntimeValue *getCurrentRuntimeValue(uint32_t Idx) {
    assert(!EventOffsetEnd && "Function has finished recording!");
    assert(Idx < CurrentValues.size() && "Bad Idx!");
    return &CurrentValues[Idx];
  }
  
  /// Get a const reference to the current RuntimeValue for an Instruction.
  /// \param Idx the index of the Instruction in the Function.
  /// \return a const reference to the RuntimeValue for the Instruction at Idx.
  RuntimeValue const *getCurrentRuntimeValue(uint32_t Idx) const {
    assert(!EventOffsetEnd && "Function has finished recording!");
    assert(Idx < CurrentValues.size() && "Bad Idx!");
    return &CurrentValues[Idx];
  }

  /// Get a reference to the current RuntimeValue for an Instruction.
  /// \param Instr the Instruction.
  /// \return a reference to the RuntimeValue for Instr.
  RuntimeValue *getCurrentRuntimeValue(llvm::Instruction const *Instr) {
    assert(!EventOffsetEnd && "Function has finished recording!");
    auto Idx = FIndex.getIndexOfInstruction(Instr);
    assert(Idx.assigned() && "Bad Instr!");
    return &CurrentValues[Idx.get<0>()];
  }
  
  /// Get a const reference to the current RuntimeValue for an Instruction.
  /// \param Instr the Instruction.
  /// \return a const reference to the RuntimeValue for Instr.
  RuntimeValue const *
  getCurrentRuntimeValue(llvm::Instruction const *Instr) const {
    assert(!EventOffsetEnd && "Function has finished recording!");
    auto Idx = FIndex.getIndexOfInstruction(Instr);
    assert(Idx.assigned() && "Bad Instr!");
    return &CurrentValues[Idx.get<0>()];
  }
  
  /// @} (Accessors for active-only information.)
  
  
  /// \name byval argument memory area tracking.
  /// @{
  
  /// \brief Add a new area for a byval argument.
  ///
  void addByValArea(MemoryArea Area);
  
  /// \brief Get all byval memory areas.
  ///
  seec::Range<decltype(ByValAreas)::const_iterator> getByValAreas() const {
    return seec::range(ByValAreas.cbegin(), ByValAreas.cend());
  }
  
  /// @} (byval argument memory area tracking.)


  /// \name Mutators
  /// @{
  
  /// \brief 
  ///
  void finishRecording(offset_uint EventOffsetEnd,
                       uint64_t ThreadTimeExited);

  /// \brief Add a new child TracedFunction.
  /// \param Child the child function call.
  void addChild(TracedFunction &Child) {
    assert(!EventOffsetEnd && "Function has finished recording!");
    
    Children.push_back(Child.RecordOffset);
  }
  
  /// \brief Add a new TracedAlloca.
  /// \param Alloca the new TracedAlloca.
  ///
  void addAlloca(TracedAlloca Alloca);
  
  /// \brief Save the current stack state for the given Key.
  ///
  void stackSave(uintptr_t Key);
  
  /// \brief Restore a previous stack state.
  /// \return area of memory that was invalidated by this stackrestore.
  ///
  MemoryArea stackRestore(uintptr_t Key);

  /// @} (Mutators)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEDFUNCTION_HPP
