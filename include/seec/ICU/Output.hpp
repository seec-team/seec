//===- include/seec/ICU/Output.hpp ---------------------------------- C++ -===//
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

#ifndef SEEC_ICU_OUTPUT_HPP
#define SEEC_ICU_OUTPUT_HPP

#include "unicode/unistr.h"

namespace llvm {

class raw_ostream;

/// Write a UnicodeString to a raw_ostream as UTF-8.
raw_ostream &operator<<(raw_ostream &Out, UnicodeString const &Str);

} // namespace llvm

#endif // SEEC_ICU_OUTPUT_HPP
