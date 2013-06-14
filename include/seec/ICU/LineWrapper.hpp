//===- include/seec/ICU/LineWrapper.hpp ----------------------------- C++ -===//
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

#ifndef SEEC_ICU_LINEWRAPPER_HPP
#define SEEC_ICU_LINEWRAPPER_HPP

#include "unicode/brkiter.h"
#include "unicode/unistr.h"

#include <functional>
#include <vector>

namespace seec {


struct WrappedLine {
  int32_t Start;
  
  int32_t End;
  
  int32_t TrailingWhitespace;
};


std::vector<WrappedLine>
wrapParagraph(BreakIterator &Breaker,
              UnicodeString const &Paragraph,
              std::function<bool (UnicodeString const &Line)> LengthCheck);


} // namespace seec

#endif // SEEC_ICU_LINEWRAPPER_HPP
