//===- lib/Clang/MappedStmt.cpp -------------------------------------------===//
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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedStmt.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Value.h"

namespace seec {

namespace seec_clang {

seec::util::Maybe<MappedStmt::Type>
getTypeFromMDString(llvm::MDString const *MDStr)
{
  if (!MDStr)
    return seec::util::Maybe<MappedStmt::Type>();
  
  auto Str = MDStr->getString();
  
  if (Str.equals("lvalsimple"))
    return MappedStmt::Type::LValSimple;
  else if (Str.equals("rvalscalar"))
    return MappedStmt::Type::RValScalar;
  else if (Str.equals("rvalaggregate"))
    return MappedStmt::Type::RValAggregate;
  
  return seec::util::Maybe<MappedStmt::Type>();
}

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

std::unique_ptr<MappedStmt>
MappedStmt::fromMetadata(llvm::MDNode *RootMD,
                         MappedModule const &Module)
{
  if (RootMD->getNumOperands() != 4u)
    return nullptr;
  
  // Get the type of the mapping.
  auto TypeMD = llvm::dyn_cast_or_null<llvm::MDString>(RootMD->getOperand(0u));
  auto Type = getTypeFromMDString(TypeMD);
  if (!Type.assigned())
    return nullptr;
  
  // Find the clang::Stmt.
  auto StmtIdentMD = llvm::dyn_cast_or_null<llvm::MDNode>
                                           (RootMD->getOperand(1u));
  if (!StmtIdentMD)
    return nullptr;
  
  auto ASTAndStmt = Module.getASTAndStmt(StmtIdentMD);
  assert(ASTAndStmt.first && ASTAndStmt.second);
  
  // Find the values.
  auto MapVal1MD = llvm::dyn_cast_or_null<llvm::MDNode>(RootMD->getOperand(2u));
  auto MapVal2MD = llvm::dyn_cast_or_null<llvm::MDNode>(RootMD->getOperand(3u));
  
  auto Val1 = getMappedValueFromMD(MapVal1MD, Module.getModuleIndex());
  auto Val2 = getMappedValueFromMD(MapVal2MD, Module.getModuleIndex());
  if (!Val1)
    return nullptr;
  
  return std::unique_ptr<MappedStmt>(new MappedStmt(Type.get<0>(),
                                                    ASTAndStmt.first,
                                                    ASTAndStmt.second,
                                                    Val1,
                                                    Val2));
}

} // namespace seec_clang (in seec)

} // namespace seec
