//===- lib/Clang/MappedLocal.cpp ------------------------------------------===//
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

#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedLocal.hpp"
#include "seec/ICU/LazyMessage.hpp"

#include "clang/AST/Decl.h"

#include "llvm/IR/Metadata.h"

namespace seec {

namespace cm {

seec::Maybe<MappedLocal, seec::Error>
MappedLocal::fromMetadata(llvm::MDNode *RootMD,
                          seec_clang::MappedModule const &Module)
{
  if (RootMD->getNumOperands() != 2u)
    return Error(LazyMessageByRef::create("SeeCClang",
                                          {"errors",
                                           "MapLocalNumOperands"}));
  
  // Find the clang::Decl.
  auto const DeclIdentMD = llvm::dyn_cast_or_null<llvm::MDNode>
                                                 (RootMD->getOperand(0u));
  if (!DeclIdentMD)
    return Error(LazyMessageByRef::create("SeeCClang",
                                          {"errors",
                                           "MapLocalInvalid"}));
  
  auto const ASTAndDecl = Module.getASTAndDecl(DeclIdentMD);
  
  if (!ASTAndDecl.first)
    return Error(LazyMessageByRef::create("SeeCClang",
                                          {"errors",
                                           "MapLocalInvalid"}));
  
  if (!ASTAndDecl.second)
    return Error(LazyMessageByRef::create("SeeCClang",
                                          {"errors",
                                           "MapLocalInvalid"}));
  
  auto const AsVarDecl = llvm::dyn_cast< ::clang::VarDecl >(ASTAndDecl.second);
  if (!AsVarDecl)
    return Error(LazyMessageByRef::create("SeeCClang",
                                          {"errors",
                                           "MapLocalInvalid"}));
  
  // Find the value.
  auto const MapValMD = llvm::dyn_cast_or_null<llvm::MDNode>
                                              (RootMD->getOperand(1u));
  
  auto const Val = seec::cm::getMappedValueFromMD(MapValMD,
                                                  Module.getModuleIndex());
  
  if (!Val)
    return Error(LazyMessageByRef::create("SeeCClang",
                                          {"errors",
                                           "MapLocalInvalid"}));
  
  return MappedLocal(AsVarDecl, Val);
}

} // namespace cm (in seec)

} // namespace seec
