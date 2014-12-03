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

#include "llvm/IR/Argument.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

namespace seec {

namespace trace {

using namespace llvm;

namespace {

Maybe<APInt> getAPIntForPointerGlobal(FunctionState const &State,
                                      unsigned const BitWidth,
                                      GlobalValue const *GV)
{
  auto const &Process = State.getParent().getParent();

  if (auto const Fn = dyn_cast<Function>(GV)) {
    if (auto const Addr = Process.getRuntimeAddress(Fn))
      return APInt(BitWidth, Addr, false);

    llvm::errs() << "don't know address of Function: " << *Fn << "\n";
    return Maybe<APInt>();
  }
  else if (auto GVar = dyn_cast<GlobalVariable>(GV)) {
    if (auto const Addr = Process.getRuntimeAddress(GVar))
      return APInt(BitWidth, Addr, false);

    llvm::errs() << "don't know address of Global: " << *GVar << "\n";
    return Maybe<APInt>();
  }

  llvm::errs() << "GlobalValue: " << *GV << "\n";
  llvm_unreachable("can't get pointer value for GlobalValue");
  return Maybe<APInt>();
}

Maybe<APInt> getAPIntForPointerArg(FunctionState const &State,
                                   unsigned const BitWidth,
                                   Argument const *Arg)
{
  if (Arg->hasByValAttr()) {
    auto const MaybeArea = State.getParamByValArea(Arg);
    if (MaybeArea.assigned<MemoryArea>())
      return APInt(BitWidth, MaybeArea.get<MemoryArea>().start(), false);

    return Maybe<APInt>();
  }

  llvm::errs() << "Argument: " << *Arg << "\n";
  llvm_unreachable("can't get pointer value for Argument");
  return Maybe<APInt>();
}

Maybe<APInt> getAPIntForPointerConstantExpr(FunctionState const &State,
                                            unsigned const BitWidth,
                                            ConstantExpr const *CE)
{
  // Handle some ConstantExpr operations. A better way to do this, if we
  // can, would be to replace the used values that are runtime constants
  // (e.g. global variable addresses) with simple constants and get LLVM
  // to deduce the value.
  if (CE->getOpcode() == Instruction::MemoryOps::GetElementPtr) {
    auto const &Process = State.getParent().getParent();
    auto const &DL = Process.getDataLayout();
    auto const Base = CE->getOperand(0);

    auto MaybeElemAddress = getAPInt(State, Base);
    if (!MaybeElemAddress.assigned<APInt>()) {
      llvm::errs() << "no value for: " << *Base << "\n";
      return Maybe<APInt>();
    }

    llvm::APInt ElemAddress = MaybeElemAddress.move<APInt>();
    llvm::Type *ElemType = Base->getType();
    auto const NumOperands = CE->getNumOperands();

    for (unsigned i = 1; i < NumOperands; ++i) {
      if (auto const ST = dyn_cast<SequentialType>(ElemType)) {
        // SequentialType indices are signed and can have any width, but
        // practically speaking are limited to i64.
        auto const MaybeValue = getAPSIntSigned(State, CE->getOperand(i));
        if (!MaybeValue.assigned<APSInt>()) {
          llvm::errs() << "no value for: " << *(CE->getOperand(i)) << "\n";
          return Maybe<APInt>();
        }

        ElemType = ST->getElementType();
        auto const ElemSize = DL.getTypeAllocSize(ElemType);
        auto const Offset = MaybeValue.get<APSInt>().getSExtValue() * ElemSize;

        if (Offset >= 0)
          ElemAddress = ElemAddress + static_cast<uint64_t>(Offset);
        else
          ElemAddress = ElemAddress - static_cast<uint64_t>(-Offset);
      }
      else if (auto const ST = dyn_cast<StructType>(ElemType)) {
        // All struct indices are i32 unsigned.
        auto const MaybeValue = getAPInt(State, CE->getOperand(i));
        if (!MaybeValue.assigned<APInt>()) {
          llvm::errs() << "no value for: " << *(CE->getOperand(i)) << "\n";
          return Maybe<APInt>();
        }

        auto const Elem = MaybeValue.get<APInt>().getZExtValue();
        auto const Layout = DL.getStructLayout(ST);
        ElemType = ST->getElementType(Elem);
        ElemAddress = ElemAddress + Layout->getElementOffset(Elem);
      }
    }

    return ElemAddress;
  }

  llvm::errs() << "can't get value for " << *CE << "\n";
  return Maybe<APInt>();
}

} // anonymous namespace

Maybe<APInt> getAPInt(FunctionState const &State, Value const *V)
{
  if (auto const IntTy = dyn_cast<IntegerType>(V->getType()))
  {
    if (auto const I = dyn_cast<Instruction>(V)) {
      auto const Value = State.getValueUInt64(I);
      if (Value.assigned<uint64_t>()) {
        return APInt(IntTy->getBitWidth(), Value.get<uint64_t>());
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

    // If this is an Instruction, get the recorded runtime value.
    if (auto const I = dyn_cast<Instruction>(V)) {
      auto const Value = State.getValuePtr(I);
      if (Value.assigned<stateptr_ty>()) {
        return APInt(BitWidth, Value.get<stateptr_ty>());
      }

      return Maybe<APInt>();
    }

    auto const StrippedValue = V->stripPointerCasts();

    if (isa<ConstantPointerNull>(StrippedValue))
      return APInt(BitWidth, 0, false);
    else if (auto const Global = dyn_cast<GlobalValue>(StrippedValue))
      return getAPIntForPointerGlobal(State, BitWidth, Global);
    else if (auto const Arg = dyn_cast<Argument>(StrippedValue))
      return getAPIntForPointerArg(State, BitWidth, Arg);
    else if (auto const CE = dyn_cast<ConstantExpr>(V))
      return getAPIntForPointerConstantExpr(State, BitWidth, CE);
    else
      llvm::errs() << "can't get value of pointer: " << *V << "\n";
  }
  else
  {
    return Maybe<APInt>();
  }

  llvm_unreachable("don't know how to extract APInt");
  return Maybe<APInt>();
}

Maybe<APSInt> getAPSIntUnsigned(FunctionState const &State, Value const *V)
{
  auto MaybeValue = getAPInt(State, V);
  if (MaybeValue.assigned<APInt>())
    return APSInt(MaybeValue.move<APInt>(), /* isUnsigned */ true);

  return Maybe<APSInt>();
}

Maybe<APSInt> getAPSIntSigned(FunctionState const &State, Value const *V)
{
  auto const IntTy = dyn_cast<IntegerType>(V->getType());
  if (!IntTy)
    return Maybe<APSInt>();

  if (auto const I = dyn_cast<Instruction>(V)) {
    auto const MaybeValue = State.getValueUInt64(I);
    if (MaybeValue.assigned<uint64_t>()) {
      return APSInt(APInt(IntTy->getBitWidth(), MaybeValue.get<uint64_t>()),
                    /* isUnsigned*/ false);
    }

    return Maybe<APSInt>();
  }
  else if (auto const CI = dyn_cast<ConstantInt>(V)) {
    return APSInt(CI->getValue(), /* isUnsigned */ false);
  }

  llvm_unreachable("don't know how to extract APSInt");
  return Maybe<APSInt>();
}

Maybe<APFloat> getAPFloat(FunctionState const &State, Value const *V)
{
  auto const TheType = V->getType();
  if (!TheType->isFloatingPointTy())
    return Maybe<APFloat>();

  if (auto const I = dyn_cast<Instruction>(V)) {
    if (TheType->isFloatTy()) {
      auto const MaybeValue = State.getValueFloat(I);
      if (MaybeValue.assigned<float>()) {
        return APFloat(MaybeValue.get<float>());
      }
    }
    else if (TheType->isDoubleTy()) {
      auto const MaybeValue = State.getValueDouble(I);
      if (MaybeValue.assigned<double>()) {
        return APFloat(MaybeValue.get<double>());
      }
    }
    else if (TheType->isX86_FP80Ty() || TheType->isFP128Ty()
             || TheType->isPPC_FP128Ty())
    {
      return State.getValueAPFloat(I);
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
