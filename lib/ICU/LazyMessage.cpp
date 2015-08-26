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
    return UnicodeString("<LazyMessage error: couldn't open bundle \"")
           + Package + "\">";
  
  // Traverse the bundles according to the keys.
  for (auto const &Key : Keys) {
    Bundle = Bundle.get(Key, Status);
    if (U_FAILURE(Status))
      return UnicodeString("<LazyMessage error: couldn't get bundle \"")
             + Key + "\">";
  }
  
  // Get the final bundle as a string.
  UnicodeString FormatString = Bundle.getString(Status);
  if (U_FAILURE(Status))
    return UnicodeString("<LazyMessage error: couldn't get string>");
  
  // Format the string using the arguments.
  MessageFormat Formatter(FormatString, Status);
  if (U_FAILURE(Status))
    return UnicodeString("<LazyMessage error: couldn't create MessageFormat>");
  
  UnicodeString Result;
  
  Formatter.format(ArgumentNames.data(),
                   ArgumentValues.data(),
                   ArgumentNames.size(),
                   Result,
                   Status);
  
  if (U_FAILURE(Status))
    return UnicodeString("<LazyMessage error: couldn't format string>");
  
  return Result;
}

UnicodeString LazyMessageByRef::describe() const {
  UnicodeString Description;
  
  Description += "<Package=";
  Description += Package;
  
  if (Keys.size()) {
    Description += ", Keys=";
    Description += Keys[0];
    for (std::size_t i = 1; i < Keys.size(); ++i) {
      Description += "/";
      Description += Keys[i];
    }
  }
  
  if (ArgumentNames.size()) {
    Description += ", Arguments=";
    Description += "(";
    Description += ArgumentNames[0];
    Description += ")";
    
    for (std::size_t i = 1; i < ArgumentNames.size(); ++i) {
      Description += ",(";
      Description += ArgumentNames[i];
      
      if (ArgumentValues[i].getType() == icu::Formattable::kString) {
        Description += ": ";
        Description += ArgumentValues[i].getString();
      }
      
      Description += ")";
    }
  }
  
  Description += ">";
  
  return Description;
}


} // namespace seec
