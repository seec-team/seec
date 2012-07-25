//===- include/seec/ICU/Format.hpp ---------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_ICU_FORMAT_HPP
#define SEEC_ICU_FORMAT_HPP

#include "unicode/fmtable.h"
#include "unicode/unistr.h"
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
