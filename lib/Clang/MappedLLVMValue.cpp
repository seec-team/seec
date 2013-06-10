//===- lib/Clang/MappedLLVMValue.cpp --------------------------------------===//
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

#include "MappedLLVMValue.hpp"

#include "seec/Util/ModuleIndex.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Value.h"

namespace seec {

namespace cm {

llvm::Value const *
getMappedValueFromMD(llvm::MDNode const *ValueMap,
                     ModuleIndex const &ModIndex)
{
  if (!ValueMap || ValueMap->getNumOperands() == 0) {
    return nullptr;
  }
  
  auto Type = llvm::dyn_cast<llvm::MDString>(ValueMap->getOperand(0u));
  auto TypeStr = Type->getString();
  
  if (TypeStr.equals("instruction")) {
    assert(ValueMap->getNumOperands() == 3);
    auto Func = llvm::dyn_cast<llvm::Function>(ValueMap->getOperand(1u));
    auto Idx = llvm::dyn_cast<llvm::ConstantInt>(ValueMap->getOperand(2u));
    assert(Func && Idx);
    auto FuncIndex = ModIndex.getFunctionIndex(Func);
    assert(FuncIndex);
    auto IdxValue = static_cast<uint32_t>(Idx->getZExtValue());
    return FuncIndex->getInstruction(IdxValue);
  }
  else if (TypeStr.equals("value")) {
    assert(ValueMap->getNumOperands() == 2);
    return ValueMap->getOperand(1u);
  }
  else if (TypeStr.equals("argument")) {
    assert(ValueMap->getNumOperands() == 3);
    auto Func = llvm::dyn_cast<llvm::Function>(ValueMap->getOperand(1u));
    auto Idx = llvm::dyn_cast<llvm::ConstantInt>(ValueMap->getOperand(2u));
    assert(Func && Idx);
    auto FuncIndex = ModIndex.getFunctionIndex(Func);
    assert(FuncIndex);
    auto IdxValue = static_cast<uint32_t>(Idx->getZExtValue());
    return FuncIndex->getArgument(IdxValue);
  }
  else {
    llvm_unreachable("Encountered unknown value type.");
    return nullptr;
  }
}

} // namespace cm (in seec)

} // namespace seec
