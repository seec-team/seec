//===- lib/ICU/Augmenter.cpp ----------------------------------------------===//
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

#include "seec/ICU/Augmenter.hpp"

#include <unicode/regex.h>

#include <cassert>

namespace seec {

UnicodeString augment(UnicodeString String, AugmentationCallbackFn Augmenter)
{
  if (String.isBogus())
    return String;

  // Example of our augmentation marker: $[concept:enum]
  UErrorCode Status = U_ZERO_ERROR;
  icu::RegexMatcher Matcher("\\$\\[([^:]*)\\:([^\\]]*)\\]", 0, Status);
  if (U_FAILURE(Status))
    return String;

  Matcher.reset(String);

  while (Matcher.find()) {
    auto const Type = Matcher.group(1, Status);
    auto const Name = Matcher.group(2, Status);
    auto const Start = Matcher.start(Status);
    auto const End = Matcher.end(Status);
    assert(U_SUCCESS(Status));

    if (Augmenter) {
      String.replaceBetween(Start, End, Augmenter(Type, Name));
    }
    else {
      String.removeBetween(Start, End);
    }

    Matcher.reset(String);
  }

  return String;
}

} // namespace seec
