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

#include "seec/ICU/LazyMessage.hpp"

#include <memory>


namespace seec {


class Error {
private:
  std::unique_ptr<seec::LazyMessage> Message;
  
public:
  Error(std::unique_ptr<seec::LazyMessage> WithMessage)
  : Message(std::move(WithMessage))
  {}
  
  Error(Error const &) = delete;
  
  Error(Error &&) = default;
  
  Error &operator=(Error const &) = delete;
  
  Error &operator=(Error &&) = default;
  
  UnicodeString getMessage(UErrorCode &Status, Locale const &GetLocale) const {
    if (U_FAILURE(Status))
      return UnicodeString();
    
    UnicodeString Msg = Message->get(Status, GetLocale);
    if (U_FAILURE(Status)) {
      return UnicodeString::fromUTF8("Couldn't load error message: ")
             + Message->describe();
    }
    
    return Msg;
  }
};


} // namespace seec


#endif // SEEC_UTIL_ERROR_HPP
