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

#ifndef SEEC_LIB_CLANG_MAPPEDLLVMVALUE_HPP
#define SEEC_LIB_CLANG_MAPPEDLLVMVALUE_HPP


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


#endif // SEEC_LIB_CLANG_MAPPEDLLVMVALUE_HPP
