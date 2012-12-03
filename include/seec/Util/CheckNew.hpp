//===- seec/Util/CheckNew.hpp --------------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_CHECKNEW_HPP
#define SEEC_UTIL_CHECKNEW_HPP

#include <cassert>

namespace seec {

inline void checkNew(bool Value) {
  if (!Value) {
    assert(false && "Memory allocation failed.");
    exit(EXIT_FAILURE);
  }
}
  
} // namespace seec

#endif // SEEC_UTIL_CHECKNEW_HPP
