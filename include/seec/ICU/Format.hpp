//===- include/seec/ICU/Format.hpp ---------------------------------- C++ -===//
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

#ifndef SEEC_ICU_FORMAT_HPP
#define SEEC_ICU_FORMAT_HPP

#include "unicode/fmtable.h"
#include "unicode/msgfmt.h"
#include "unicode/msgfmt.h"

namespace seec {

template<typename... Ts>
UnicodeString format(UnicodeString FormatString,
                     UErrorCode &Status,
                     Ts... Arguments)
{
  UnicodeString Result;

  Formattable FmtArguments[] = {
    Formattable(Arguments)...
  };

  Result = MessageFormat::format(FormatString,
                                 FmtArguments,
                                 sizeof...(Ts),
                                 Result,
                                 Status);

  return Result;
}

}

#endif // SEEC_ICU_FORMAT_HPP
