//===- include/seec/Clang/RuntimeValueMapping.hpp -------------------------===//
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

#ifndef SEEC_CLANG_RUNTIMEVALUEMAPPING_HPP
#define SEEC_CLANG_RUNTIMEVALUEMAPPING_HPP

#include <string>

namespace clang {
  class Stmt;
}

namespace llvm {
  class Instruction;
}

namespace seec {
  namespace trace {
    class RuntimeValue;
  }

namespace seec_clang {


/// \brief Get a string representation of the value produced by an
///        llvm::Instruction belonging to a clang::Stmt.
///
std::string toString(clang::Stmt const *Statement,
                     llvm::Instruction const *Instruction,
                     seec::trace::RuntimeValue const &Value);


} // namespace seec_clang (in seec)

} // namespace seec


#endif // SEEC_CLANG_RUNTIMEVALUEMAPPING_HPP
