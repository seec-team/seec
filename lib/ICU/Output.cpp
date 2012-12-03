//===- lib/ICU/Output.cpp -------------------------------------------------===//
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

#include "seec/ICU/Output.hpp"

#include "llvm/Support/raw_ostream.h"

#include "unicode/unistr.h"

#include <string>

namespace seec {

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              UnicodeString const &Str) {
  std::string Buffer;
  Str.toUTF8String(Buffer);
  Out << Buffer;
  return Out;
}

} // namespace seec
