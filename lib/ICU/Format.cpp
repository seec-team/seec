//===- lib/ICU/Format.cpp -------------------------------------------------===//
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


#include "seec/ICU/Format.hpp"


namespace seec {

namespace icu {


UnicodeString format(UnicodeString const &FormatString,
                     FormatArgumentsWithNames const &Arguments,
                     UErrorCode &Status)
{
  UnicodeString Result;
  
  if (U_FAILURE(Status)) {
    Result.setToBogus();
    return Result;
  }
  
  MessageFormat Formatter(FormatString, Status);
  
  if (U_FAILURE(Status)) {
    Result.setToBogus();
    return Result;
  }
  
  Formatter.format(Arguments.names(),
                   Arguments.values(),
                   Arguments.size(),
                   Result,
                   Status);
  
  return Result;
}


} // namespace icu (in seec)

} // namespace seec
