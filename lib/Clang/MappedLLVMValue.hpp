//===- lib/Clang/MappedLLVMValue.hpp --------------------------------------===//
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

namespace llvm {
  class MDNode;
  class Value;
}

namespace seec {

class ModuleIndex;

namespace cm {

llvm::Value const *
getMappedValueFromMD(llvm::MDNode const *ValueMap,
                     ModuleIndex const &ModIndex);

} // namespace cm (in seec)

} // namespace seec
