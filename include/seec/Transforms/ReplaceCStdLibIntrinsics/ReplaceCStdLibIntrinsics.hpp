//===- ReplaceCStdLibIntrinsics.hpp --------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This pass replaces LLVM's intrinsics for C standard library functions, with
// normal calls to the standard library functions.
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_REPLACE_CSTDLIB_INTRINSICS_HPP_
#define _SEEC_REPLACE_CSTDLIB_INTRINSICS_HPP_

#include "llvm/Pass.h"
#include "llvm/Support/InstVisitor.h"

namespace llvm {

class Module;
class Function;
class AnalysisUsage;
class CallInst;

class ReplaceCStdLibIntrinsics :
  public FunctionPass, public InstVisitor<ReplaceCStdLibIntrinsics, bool> {
private:

#define SEEC_CSTDLIB_INTRINSIC(INTRINSIC_NAME, C_NAME, LLVM_TYPE, NARGS, ...) \
  Function *C##C_NAME;
#include "CStdLibIntrinsics.def"

public:
  static char ID; ///< For LLVM's RTTI

  /// Construct a new ReplaceCStdLibIntrinsics pass.
  ReplaceCStdLibIntrinsics()
  : FunctionPass(ID)
  {}

  /// Get the name of this pass.
  /// \return A string containing the name of this pass.
  const char *getPassName() const {
    return "Replace LLVM Intrinsics with calls to C Standard Library";
  }

  virtual bool doInitialization(Module &M);

  virtual bool doFinalization(Module &M);

  virtual bool runOnFunction(Function &F);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  bool visitCallInst(CallInst &I);

  bool visitInstruction(Instruction &I) { return false; }
}; // class ReplaceCStdLibIntrinsics

} // namespace llvm

#endif // _SEEC_REPLACE_CSTDLIB_INTRINSICS_HPP_
