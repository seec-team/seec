//===- lib/Util/Error.cpp -------------------------------------------------===//
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
#include "seec/Util/Error.hpp"

#include "unicode/locid.h"


namespace seec {

UnicodeString getOrDescribe(Error const &Err)
{
  UErrorCode Status = U_ZERO_ERROR;
  auto const Message = Err.getMessage(Status, Locale());
  if (U_SUCCESS(Status))
    return Message;
  else
    return Err.describeMessage();
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, Error const &Err)
{
  UErrorCode Status = U_ZERO_ERROR;
  auto const Message = Err.getMessage(Status, Locale());
  if (U_SUCCESS(Status))
    return Out << Message;
  else
    return Out << Err.describeMessage();
}

} // namespace seec
