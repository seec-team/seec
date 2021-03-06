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

#include "MappedLLVMValue.hpp"

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedStmt.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Value.h"

namespace seec {

namespace seec_clang {

//===----------------------------------------------------------------------===//
// MappedStmt
//===----------------------------------------------------------------------===//

seec::Maybe<MappedStmt::Type>
getTypeFromMDString(llvm::MDString const *MDStr)
{
  if (!MDStr)
    return seec::Maybe<MappedStmt::Type>();
  
  auto Str = MDStr->getString();
  
  if (Str.equals("lvalsimple"))
    return MappedStmt::Type::LValSimple;
  else if (Str.equals("rvalscalar"))
    return MappedStmt::Type::RValScalar;
  else if (Str.equals("rvalaggregate"))
    return MappedStmt::Type::RValAggregate;
  
  return seec::Maybe<MappedStmt::Type>();
}

std::unique_ptr<MappedStmt>
MappedStmt::fromMetadata(llvm::MDNode *RootMD,
                         MappedModule const &Module)
{
  if (RootMD->getNumOperands() != 4u) {
    llvm::errs() << "MappedStmt::fromMetadata(): "
                 << "invalid number of operands.\n";
    
    return nullptr;
  }
  
  // Get the type of the mapping.
  auto TypeMD = llvm::dyn_cast_or_null<llvm::MDString>
                                      (RootMD->getOperand(0u).get());
  auto Type = getTypeFromMDString(TypeMD);
  if (!Type.assigned()) {
    llvm::errs() << "MappedStmt::fromMetadata(): "
                 << "failed to get type.\n";
    
    return nullptr;
  }
  
  // Find the clang::Stmt.
  auto StmtIdentMD = llvm::dyn_cast_or_null<llvm::MDNode>
                                           (RootMD->getOperand(1u).get());
  if (!StmtIdentMD) {
    llvm::errs() << "MappedStmt::fromMetadata(): "
                 << "Stmt identifier is not an MDNode.\n";
    
    return nullptr;
  }
  
  auto ASTAndStmt = Module.getASTAndStmt(StmtIdentMD);
  assert(ASTAndStmt.first && ASTAndStmt.second);
  
  // Find the values.
  auto const MapVal1MD = RootMD->getOperand(2u).get();
  auto const MapVal2MD = RootMD->getOperand(3u).get();
  
  auto Val1 = seec::cm::getMappedValueFromMD(MapVal1MD,
                                             Module.getModuleIndex());
  auto Val2 = seec::cm::getMappedValueFromMD(MapVal2MD,
                                             Module.getModuleIndex());
  if (!Val1) {
    llvm::errs() << "MappedStmt::fromMetadata(): "
                 << "llvm::Value not found.\n";
    
    return nullptr;
  }
  
  return std::unique_ptr<MappedStmt>(new MappedStmt(Type.get<0>(),
                                                    ASTAndStmt.first,
                                                    ASTAndStmt.second,
                                                    Val1,
                                                    Val2));
}

//===----------------------------------------------------------------------===//
// getAllChildren()
//===----------------------------------------------------------------------===//

static void
addAllChildren(llvm::DenseSet<clang::Stmt const *> &Set, clang::Stmt const *S)
{
  for (auto const &Child : S->children()) {
    if (Child) {
      Set.insert(Child);
      addAllChildren(Set, Child);
    }
  }
}

llvm::DenseSet<clang::Stmt const *> getAllChildren(clang::Stmt const *S)
{
  llvm::DenseSet<clang::Stmt const *> Set;
  addAllChildren(Set, S);
  return Set;
}

} // namespace seec_clang (in seec)

} // namespace seec
