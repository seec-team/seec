//===- tools/seec-trace-view/ValueFormat.hpp ------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_VALUEFORMAT_HPP
#define SEEC_TRACE_VIEW_VALUEFORMAT_HPP

#include "unicode/unistr.h"


namespace seec {
  namespace cm {
    class ProcessState;
    class Value;
  }
}


UnicodeString getPrettyStringForInline(seec::cm::Value const &Value,
                                       seec::cm::ProcessState const &State);

UnicodeString shortenValueString(UnicodeString ValueString, int32_t Length);


#endif // SEEC_TRACE_VIEW_VALUEFORMAT_HPP
