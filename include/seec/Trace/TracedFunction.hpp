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
#include "seec/Trace/TracePointer.hpp"
#include "seec/Util/IndexTypesForLLVMObjects.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/ADT/DenseMap.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace llvm {

class AllocaInst;
class CallInst;
class DataLayout;
class Instruction;

} // namespace llvm

namespace seec {

namespace trace {

class TraceMemoryState;
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


/// \brief Stores information about a single recorded byval parameter.
///
class TracedParamByVal {
  /// The parameter's llvm::Argument
  llvm::Argument const *Arg;

  /// The memory area occupied by the parameter.
  MemoryArea Area;

public:
  /// \brief Constructor.
  ///
  TracedParamByVal(llvm::Argument const * const ForArg,
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


/// \brief Stores the record information for an executed Function.
///
///
class RecordedFunction {
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

public:
  /// \brief Constructor.
  ///
  RecordedFunction(offset_uint const WithRecordOffset,
                   uint32_t const WithIndex,
                   offset_uint const WithEventOffsetStart,
                   uint64_t const WithThreadTimeEntered)
  : RecordOffset(WithRecordOffset),
    Index(WithIndex),
    EventOffsetStart(WithEventOffsetStart),
    EventOffsetEnd(0),
    ThreadTimeEntered(WithThreadTimeEntered),
    ThreadTimeExited(0),
    Children()
  {}

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

  void addChild(RecordedFunction const &Child)
  {
    assert(EventOffsetEnd == 0 && ThreadTimeExited == 0);
    Children.push_back(Child.RecordOffset);
  }

  void setCompletion(offset_uint const WithEventOffsetEnd,
                     uint64_t const WithThreadTimeExited);
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
  FunctionIndex const *FIndex;

  /// This Function execution's \c RecordedFunction. If this \c TracedFunction
  /// is a shim, then this is the parent's \c RecordedFunction.
  RecordedFunction &Record;

  /// @}
  
  
  /// \name Active-only information.
  /// @{
  
  /// Currently-active Instruction.
  llvm::Instruction const *ActiveInstruction;
  
  /// Previously active \c BasicBlock.
  llvm::BasicBlock const *PreviousBasicBlock;

  /// Currently active \c BasicBlock.
  llvm::BasicBlock const *ActiveBasicBlock;

  /// List of Allocas for this function.
  std::vector<TracedAlloca> Allocas;
  
  /// Areas occupied by byval arguments for this function.
  std::vector<TracedParamByVal> ByValArgs;
  
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
  
  /// Pointer objects of Arguments.
  llvm::DenseMap<llvm::Argument const *, PointerTarget> ArgPointerObjects;

  /// Pointer objects (original pointee of the pointer).
  llvm::DenseMap<llvm::Instruction const *, PointerTarget> PointerObjects;

  /// @}
  

public:
  /// \brief Constructor.
  ///
  TracedFunction(TraceThreadListener const &WithThreadListener,
                 FunctionIndex &WithFIndex,
                 RecordedFunction &WithRecord,
                 llvm::DenseMap<llvm::Argument const *, PointerTarget> ArgPtrs)
  : ThreadListener(WithThreadListener),
    FIndex(&WithFIndex),
    Record(WithRecord),
    ActiveInstruction(nullptr),
    PreviousBasicBlock(nullptr),
    ActiveBasicBlock(nullptr),
    Allocas(),
    ByValArgs(),
    StackSaves(),
    StackLow(0),
    StackHigh(0),
    CurrentValues(FIndex->getInstructionCount()),
    ArgPointerObjects(std::move(ArgPtrs)),
    PointerObjects()
  {}

  /// \brief Constructor for shims.
  ///
  TracedFunction(TraceThreadListener &WithThreadListener,
                 RecordedFunction &WithParentRecord)
  : ThreadListener(WithThreadListener),
    FIndex(nullptr),
    Record(WithParentRecord),
    ActiveInstruction(nullptr),
    PreviousBasicBlock(nullptr),
    ActiveBasicBlock(nullptr),
    Allocas(),
    ByValArgs(),
    StackSaves(),
    StackLow(),
    StackHigh(),
    CurrentValues(),
    ArgPointerObjects(),
    PointerObjects()
  {}

  /// \brief Move constructor.
  ///
  TracedFunction(TracedFunction &&Other)
  : ThreadListener(Other.ThreadListener),
    FIndex(Other.FIndex),
    Record(Other.Record),
    ActiveInstruction(Other.ActiveInstruction),
    PreviousBasicBlock(Other.PreviousBasicBlock),
    ActiveBasicBlock(Other.ActiveBasicBlock),
    Allocas(std::move(Other.Allocas)),
    ByValArgs(std::move(Other.ByValArgs)),
    StackSaves(std::move(Other.StackSaves)),
    StackLow(Other.StackLow),
    StackHigh(Other.StackHigh),
    CurrentValues(std::move(Other.CurrentValues)),
    ArgPointerObjects(std::move(Other.ArgPointerObjects)),
    PointerObjects(std::move(Other.PointerObjects))
  {}


  /// \name Accessors for permanent information.
  /// @{

  /// \brief Check if this is a shim.
  ///
  /// A shim has no \c FunctionIndex, and should only interact with child
  /// function's \c notifyFunctionBegin() and \c notifyFunctionEnd() calls.
  ///
  /// A shim holds the pointer objects for arguments passed to the child
  /// function, but because there is no \c FunctionIndex they are mapped to
  /// the \c llvm::Argument pointers for the child, rather than needing to
  /// extract them from the appropriate argument's \c llvm::Value. This means
  /// that a shim's \c getPointerObject(llvm::Argument) retrieves the object
  /// for a child call's argument, rather than one of the shim's arguments.
  ///
  bool isShim() const { return FIndex == nullptr; }

  /// Get FunctionIndex for the recorded Function.
  FunctionIndex const &getFunctionIndex() const {
    assert(FIndex && "Incorrect usage of TracedFunction shim!");
    return *FIndex;
  }

  /// Get the \c RecordedFunction for this Function's execution.
  RecordedFunction &getRecordedFunction() { return Record; }

  /// @} (Accessors for permanent information.)
  
  
  /// \name Support getCurrentRuntimeValue.
  /// @{
  
  /// \brief Get the \c llvm::DataLayout for the \c llvm::Module.
  ///
  llvm::DataLayout const &getDataLayout() const;

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

    auto const BB = ActiveInstruction->getParent();
    if (BB != ActiveBasicBlock) {
      PreviousBasicBlock = ActiveBasicBlock;
      ActiveBasicBlock   = BB;
    }
  }
  
  /// \brief Clear the currently active llvm::Instruction.
  ///
  void clearActiveInstruction() {
    ActiveInstruction = nullptr;
  }

  /// \brief Get the previously active \c llvm::BasicBlock.
  ///
  llvm::BasicBlock const *getPreviousBasicBlock() const {
    return PreviousBasicBlock;
  }

  /// \brief Get the currently active \c llvm::BasicBlock.
  ///
  llvm::BasicBlock const *getActiveBasicBlock() const {
    return ActiveBasicBlock;
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
    return MemoryArea(StackLow, (StackHigh - StackLow) + 1);
  }
  
  /// Get the stack-allocated area that contains an address. This method is
  /// thread safe.
  seec::Maybe<MemoryArea>
  getContainingMemoryArea(uintptr_t Address) const;
  
  /// Get a reference to the current RuntimeValue for an Instruction.
  /// \param Idx the index of the Instruction in the Function.
  /// \return a reference to the RuntimeValue for the Instruction at Idx.
  RuntimeValue *getCurrentRuntimeValue(InstrIndexInFn Idx) {
    assert(Idx < CurrentValues.size() && "Bad Idx!");
    return &CurrentValues[Idx.raw()];
  }
  
  /// Get a const reference to the current RuntimeValue for an Instruction.
  /// \param Idx the index of the Instruction in the Function.
  /// \return a const reference to the RuntimeValue for the Instruction at Idx.
  RuntimeValue const *getCurrentRuntimeValue(InstrIndexInFn Idx) const {
    assert(Idx < CurrentValues.size() && "Bad Idx!");
    return &CurrentValues[Idx.raw()];
  }

  /// Get a reference to the current RuntimeValue for an Instruction.
  /// \param Instr the Instruction.
  /// \return a reference to the RuntimeValue for Instr.
  RuntimeValue *getCurrentRuntimeValue(llvm::Instruction const *Instr) {
    assert(FIndex && "Incorrect usage of TracedFunction shim!");
    auto const Idx = FIndex->getIndexOfInstruction(Instr)->raw();
    return &CurrentValues[Idx];
  }
  
  /// Get a const reference to the current RuntimeValue for an Instruction.
  /// \param Instr the Instruction.
  /// \return a const reference to the RuntimeValue for Instr.
  RuntimeValue const *
  getCurrentRuntimeValue(llvm::Instruction const *Instr) const {
    assert(FIndex && "Incorrect usage of TracedFunction shim!");
    auto const Idx = FIndex->getIndexOfInstruction(Instr)->raw();
    return &CurrentValues[Idx];
  }
  
  /// @} (Accessors for active-only information.)
  
  
  /// \name byval argument memory area tracking.
  /// @{
  
  /// \brief Add a new area for a byval argument.
  ///
  void addByValArg(llvm::Argument const * const Arg, MemoryArea const &Area);
  
  /// \brief Get the area occupied by the given byval Arg.
  /// For getCurrentRuntimeValueAs().
  ///
  seec::Maybe<seec::MemoryArea>
  getParamByValArea(llvm::Argument const *Arg) const;
  
  /// \brief Get all byval memory areas.
  ///
  seec::Range<decltype(ByValArgs)::const_iterator> getByValArgs() const {
    return seec::range(ByValArgs.cbegin(), ByValArgs.cend());
  }
  
  /// @} (byval argument memory area tracking.)


  /// \name Pointer object tracking.
  /// @{

  /// \brief Get the object of the pointer held by an \c Argument.
  ///
  PointerTarget getPointerObject(llvm::Argument const *A) const;

  /// \brief Set the object of a pointer held by an \c Argument.
  ///
  void setPointerObject(llvm::Argument const *A, PointerTarget const &Object);

  /// \brief Get the object of the pointer produced by an \c Instruction.
  ///
  PointerTarget getPointerObject(llvm::Instruction const *I) const;

  /// \brief Set the object of a pointer produced by an \c Instruction.
  ///
  void setPointerObject(llvm::Instruction const *I,
                        PointerTarget const &Object);

  /// \brief Get the object of a general pointer.
  /// If the given value is an \c Instruction, then we will search for the
  /// object of that \c Instruction as recorded in this \c Function execution.
  ///
  PointerTarget getPointerObject(llvm::Value const *V) const;

  /// \brief Transfer a pointer object from a \c Value to an \c Instruction.
  ///
  PointerTarget transferPointerObject(llvm::Value const *From,
                                      llvm::Instruction const *To);

  /// \brief Transfer a pointer object from one of the active \c Call's
  ///        \c Argument's to the \c Call itself.
  ///
  PointerTarget transferArgPointerObjectToCall(unsigned const ArgNo);

  /// @} (Pointer object tracking.)


  /// \name Mutators
  /// @{

  /// \brief Add a new child TracedFunction.
  /// \param Child the child function call.
  void addChild(TracedFunction &Child) { Record.addChild(Child.Record); }
  
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
  void stackRestore(uintptr_t Key, TraceMemoryState &TraceMemory);

  /// @} (Mutators)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEDFUNCTION_HPP
