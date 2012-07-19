#include "seec/Trace/FunctionState.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

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
