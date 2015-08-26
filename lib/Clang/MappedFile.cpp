//===- lib/Clang/MappedFile.cpp -------------------------------------------===//
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

#include "seec/Clang/MappedFile.hpp"

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Value.h"

namespace seec {

namespace seec_clang {

seec::Maybe<MappedFile> MappedFile::fromMetadata(llvm::Metadata *Root) {
  auto MDRoot = llvm::dyn_cast<llvm::MDNode>(Root);
  if (!MDRoot || MDRoot->getNumOperands() != 2u)
    return seec::Maybe<MappedFile>();
  
  auto File = llvm::dyn_cast<llvm::MDString>(MDRoot->getOperand(0u));
  auto Directory = llvm::dyn_cast<llvm::MDString>(MDRoot->getOperand(1u));
  
  if (!File || !Directory)
    return seec::Maybe<MappedFile>();
  
  return MappedFile(File->getString().str(), Directory->getString().str());
}

} // namespace seec_clang (in seec)

} // namespace seec
