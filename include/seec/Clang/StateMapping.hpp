//===- StateMapping.hpp ---------------------------------------------------===//
//
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


std::string toString(clang::ValueDecl const *Value, llvm::ArrayRef<char> State);


} // namespace seec_clang (in seec)

} // namespace seec


#endif // SEEC_CLANG_STATEMAPPING_HPP
