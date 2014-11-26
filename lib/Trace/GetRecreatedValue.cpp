//===- lib/Trace/GetRecreatedValue.cpp ------------------------------------===//
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

#include "seec/Trace/GetRecreatedValue.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/ProcessState.hpp"

#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"

namespace seec {

namespace trace {

using namespace llvm;

Maybe<APInt> getAPInt(FunctionState const &State, Value const *V)
{
  if (V->getType()->isIntegerTy())
  {
    if (auto const I = dyn_cast<Instruction>(V)) {
      if (auto const RTV = State.getCurrentRuntimeValue(I)) {
        return RTV->getAPInt(cast<IntegerType>(V->getType()), false);
      }

      return Maybe<APInt>();
    }
    else if (auto const CI = dyn_cast<ConstantInt>(V)) {
      return CI->getValue();
    }
  }
  else if (auto const PtrTy = dyn_cast<PointerType>(V->getType()))
  {
    auto const &DL = State.getParent().getParent().getDataLayout();
    auto const BitWidth = DL.getPointerSizeInBits(PtrTy->getAddressSpace());

    if (auto const I = dyn_cast<Instruction>(V)) {
      if (auto const RTV = State.getCurrentRuntimeValue(I)) {
        return APInt(BitWidth, RTV->getUInt64(), false);
      }

      return Maybe<APInt>();
    }

    // TODO: constant pointers.
  }
  else
  {
    return Maybe<APInt>();
  }

  llvm_unreachable("don't know how to extract APInt");
  return Maybe<APInt>();
}

Maybe<APSInt> getAPSInt(FunctionState const &State, Value const *V)
{
  if (!V->getType()->isIntegerTy())
    return Maybe<APSInt>();

  if (auto const I = dyn_cast<Instruction>(V)) {
    if (auto const RTV = State.getCurrentRuntimeValue(I)) {
      return RTV->getAPSInt(cast<IntegerType>(V->getType()));
    }

    return Maybe<APSInt>();
  }
  else if (auto const CI = dyn_cast<ConstantInt>(V)) {
    return APSInt(CI->getValue());
  }

  llvm_unreachable("don't know how to extract APSInt");
  return Maybe<APSInt>();
}

Maybe<APFloat> getAPFloat(FunctionState const &State, Value const *V)
{
  if (!V->getType()->isFloatingPointTy())
    return Maybe<APFloat>();

  if (auto const I = dyn_cast<Instruction>(V)) {
    if (auto const RTV = State.getCurrentRuntimeValue(I)) {
      return RTV->getAPFloat(V->getType());
    }

    return Maybe<APFloat>();
  }
  else if (auto const CF = dyn_cast<ConstantFP>(V)) {
    return CF->getValueAPF();
  }

  return Maybe<APFloat>();
}

} // namespace trace (in seec)

} // namespace seec
