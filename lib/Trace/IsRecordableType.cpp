//===- lib/Trace/IsRecordableType.cpp -------------------------------------===//
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

#include "seec/Trace/IsRecordableType.hpp"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"


namespace seec {

namespace trace {

bool isRecordableType(llvm::Type const *T)
{
  if (auto IT = llvm::dyn_cast<llvm::IntegerType>(T)) {
    return IT->getBitWidth() <= 64;
  }
  else if (T->isPointerTy()) {
    return true;
  }
  else if (T->isFloatTy()) {
    return true;
  }
  else if (T->isDoubleTy()) {
    return true;
  }
  else if (T->isX86_FP80Ty() || T->isFP128Ty() || T->isPPC_FP128Ty()) {
    return true;
  }

  return false;
}

} // namespace trace (in seec)

} // namespace seec