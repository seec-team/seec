//===- lib/ICU/LazyMessage.cpp -------------------------------------- C++ -===//
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

#include "seec/ICU/LazyMessage.hpp"

#include "unicode/resbund.h"
#include "unicode/msgfmt.h"

namespace seec {


UnicodeString LazyMessageByRef::create(UErrorCode &Status,
                                       Locale const &GetLocale) const {
  if (U_FAILURE(Status))
    return UnicodeString();
  
  // Load the package.
  ResourceBundle Bundle(Package, GetLocale, Status);
  if (U_FAILURE(Status))
    return UnicodeString();
  
  // Traverse the bundles according to the keys.
  for (auto const &Key : Keys) {
    Bundle = Bundle.get(Key, Status);
    if (U_FAILURE(Status))
      return UnicodeString();
  }
  
  // Get the final bundle as a string.
  UnicodeString FormatString = Bundle.getString(Status);
  if (U_FAILURE(Status))
    return UnicodeString();
  
  // Format the string using the arguments.
  MessageFormat Formatter(FormatString, Status);
  if (U_FAILURE(Status))
    return UnicodeString();
  
  UnicodeString Result;
  
  Formatter.format(ArgumentNames.data(),
                   ArgumentValues.data(),
                   ArgumentNames.size(),
                   Result,
                   Status);
  
  return Result;
}


} // namespace seec
