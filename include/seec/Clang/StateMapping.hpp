//===- include/seec/Clang/StateMapping.hpp --------------------------------===//
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

#ifndef SEEC_CLANG_STATEMAPPING_HPP
#define SEEC_CLANG_STATEMAPPING_HPP

#include "llvm/ADT/ArrayRef.h"

#include <string>

namespace clang {
  class ValueDecl;
}

namespace seec {

/// Contains classes to assist with SeeC's usage of Clang.
namespace seec_clang {


/// \brief Get a string representation of State interpreted as the type of
///        Value.
std::string toString(clang::ValueDecl const *Value, llvm::ArrayRef<char> State);


} // namespace seec_clang (in seec)

} // namespace seec


#endif // SEEC_CLANG_STATEMAPPING_HPP
