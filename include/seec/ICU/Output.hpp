//===- include/seec/ICU/Output.hpp ---------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_ICU_OUTPUT_HPP
#define SEEC_ICU_OUTPUT_HPP

#include "unicode/unistr.h"

namespace llvm {

class raw_ostream;

}

namespace seec {

/// Write a UnicodeString to a raw_ostream as UTF-8.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, UnicodeString const &Str);

}

#endif // SEEC_ICU_OUTPUT_HPP
