#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/Instructions.h"
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

MemoryState::Region AllocaState::getMemoryRegion() const {
  auto &Thread = Parent->getParent();
  auto &Process = Thread.getParent();
  auto &Memory = Process.getMemory();
  return Memory.getRegion(MemoryArea(Address, getTotalSize()));
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
  InstructionValues(Function.getInstructionCount()),
  Allocas()
{
  assert(FunctionLookup);
}

llvm::Function const *FunctionState::getFunction() const {
  return Parent->getParent().getModule().getFunction(Index);
}

llvm::Instruction const *FunctionState::getActiveInstruction() const {
  if (!ActiveInstruction.assigned())
    return nullptr;

  return FunctionLookup->getInstruction(ActiveInstruction.get<0>());
}

RuntimeValue &FunctionState::getRuntimeValue(llvm::Instruction const *I) {
  auto MaybeIndex = FunctionLookup->getIndexOfInstruction(I);
  return getRuntimeValue(MaybeIndex.get<0>());
}

RuntimeValue const &
FunctionState::getRuntimeValue(llvm::Instruction const *I) const {
  auto MaybeIndex = FunctionLookup->getIndexOfInstruction(I);
  return getRuntimeValue(MaybeIndex.get<0>());
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

  Out << "   Instruction values:\n";
  auto const InstructionCount = State.getInstructionCount();
  for (std::size_t i = 0; i < InstructionCount; ++i) {
    auto const &Value = State.getRuntimeValue(i);
    if (Value.assigned()) {
      Out << "    " << i << " = "
          << Value.getUInt64() << " or "
          << Value.getFloat() << " or "
          << Value.getDouble() << "\n";
    }
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
