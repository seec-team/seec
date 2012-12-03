//===- seec/Util/Error.hpp ------------------------------------------ C++ -===//
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

#ifndef SEEC_UTIL_ERROR_HPP
#define SEEC_UTIL_ERROR_HPP

namespace seec {
  
class Error {
public:
  Error() {}
  
  virtual ~Error() {}
};

} // namespace seec

#endif // SEEC_UTIL_ERROR_HPP
