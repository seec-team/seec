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
getMappedValueFromMD(llvm::Metadata const *ValueMapMD,
                     ModuleIndex const &ModIndex)
{
  if (!ValueMapMD) {
    return nullptr;
  }

  if (auto const CMD = llvm::dyn_cast<llvm::ConstantAsMetadata>(ValueMapMD)) {
    return CMD->getValue();
  }

  auto const ValueMap = llvm::dyn_cast<llvm::MDNode>(ValueMapMD);
  if (!ValueMap || ValueMap->getNumOperands() == 0) {
    return nullptr;
  }
  
  auto Type = llvm::dyn_cast<llvm::MDString>(ValueMap->getOperand(0u));
  auto TypeStr = Type->getString();
  
  if (TypeStr.equals("instruction")) {
    assert(ValueMap->getNumOperands() == 3);

    auto FuncValMD = llvm::dyn_cast<llvm::ConstantAsMetadata>
                                   (ValueMap->getOperand(1u).get());
    if (!FuncValMD)
      return nullptr;

    auto Func = llvm::dyn_cast<llvm::Function>(FuncValMD->getValue());
    if (!Func)
      return nullptr;

    auto IdxMD = llvm::cast<llvm::ConstantAsMetadata>
                           (ValueMap->getOperand(2u).get());

    auto Idx = llvm::cast<llvm::ConstantInt>(IdxMD->getValue());

    auto FuncIndex = ModIndex.getFunctionIndex(Func);
    assert(FuncIndex);

    auto IdxValue = static_cast<uint32_t>(Idx->getZExtValue());
    return FuncIndex->getInstruction(IdxValue);
  }
  else if (TypeStr.equals("value")) {
    assert(ValueMap->getNumOperands() == 2);
    auto const MDVal = llvm::cast<llvm::ValueAsMetadata>
                                 (ValueMap->getOperand(1u).get());
    return MDVal->getValue();
  }
  else if (TypeStr.equals("argument")) {
    assert(ValueMap->getNumOperands() == 3);

    auto FuncValMD = llvm::dyn_cast<llvm::ConstantAsMetadata>
                                   (ValueMap->getOperand(1u).get());
    if (!FuncValMD)
      return nullptr;

    auto Func = llvm::dyn_cast<llvm::Function>(FuncValMD->getValue());
    if (!Func)
      return nullptr;

    auto IdxMD = llvm::cast<llvm::ConstantAsMetadata>
                           (ValueMap->getOperand(2u).get());

    auto Idx = llvm::cast<llvm::ConstantInt>(IdxMD->getValue());

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
