//===- ReplaceCStdLibIntrinsics.cpp --------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "seec"

#include "seec/Transforms/ReplaceCStdLibIntrinsics/ReplaceCStdLibIntrinsics.hpp"
#include "seec/Util/ValueConversion.hpp"

#include "llvm/DataLayout.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Type.h"
#include "llvm/TypeBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <cassert>

using namespace seec;

namespace llvm {

// class ReplaceCStdLibIntrinsics

char ReplaceCStdLibIntrinsics::ID = 0;

/// Perform module-level initialization before the pass is run.
/// \param M a reference to the LLVM module to modify.
/// \return true if this LLVM module has been modified.
bool ReplaceCStdLibIntrinsics::doInitialization(Module &M) {
  LLVMContext &Context = M.getContext();

#define SEEC_CSTDLIB_INTRINSIC(INTRINSIC_NAME, C_NAME, LLVM_TYPE, NARGS, ...) \
  C##C_NAME = cast<Function>(M.getOrInsertFunction(#C_NAME, \
    TypeBuilder<LLVM_TYPE, false>::get(Context)));
#include "seec/Transforms/ReplaceCStdLibIntrinsics/CStdLibIntrinsics.def"

  return true;
}

bool EraseFunctionPredicate(Function &F) {
  switch (F.getIntrinsicID()) {
    case Intrinsic::not_intrinsic: return false;
#define SEEC_CSTDLIB_INTRINSIC(INTRINSIC_NAME, C_NAME, LLVM_TYPE, NARGS, ...) \
    case Intrinsic::INTRINSIC_NAME: return true;
#include "seec/Transforms/ReplaceCStdLibIntrinsics/CStdLibIntrinsics.def"
    default: return false;
  }
}

///
bool ReplaceCStdLibIntrinsics::doFinalization(Module &M) {
  M.getFunctionList().erase_if(EraseFunctionPredicate);
  return true;
}

/// Replace intrinsics in a single Function.
/// \param F the Function.
/// \return true if the Function was modified (i.e. any intrinsic calls were
///         found and replaced).
bool ReplaceCStdLibIntrinsics::runOnFunction(Function &F) {
  bool Modified = false;

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    bool Replaced = visit(*I);

    if (Replaced) {
      Modified = true;
      (I++)->eraseFromParent();
    }
    else
      ++I;
  }

  return Modified;
}

void ReplaceCStdLibIntrinsics::getAnalysisUsage(AnalysisUsage &AU) const {

}

bool ReplaceCStdLibIntrinsics::visitCallInst(CallInst &I) {
  Function *F = I.getCalledFunction();
  if (!F)
    return false;

  switch (F->getIntrinsicID()) {
    case Intrinsic::not_intrinsic:
      return false;

#define SEEC_CSTDLIB_INTRINSIC_ARG(TYPE, OPERAND) \
    getValueAs<TYPE, InsertPosition::After>(I.getArgOperand(OPERAND), &I)
#define SEEC_CSTDLIB_INTRINSIC(INTRINSIC_NAME, C_NAME, LLVM_TYPE, NARGS, ...) \
    case Intrinsic::INTRINSIC_NAME: \
      { \
        Value *Args[NARGS] = { __VA_ARGS__ }; \
        CallInst *Replacement = CallInst::Create(C##C_NAME, Args); \
        Replacement->insertAfter(&I); \
        return true; \
      }
#include "seec/Transforms/ReplaceCStdLibIntrinsics/CStdLibIntrinsics.def"

    default:
      return false;
  }

  return false;
}

} // namespace llvm
