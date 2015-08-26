//===- include/seec/ICU/Augmenter.hpp ------------------------------- C++ -===//
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

#ifndef SEEC_ICU_AUGMENTER_HPP
#define SEEC_ICU_AUGMENTER_HPP

#include <unicode/unistr.h>

#include <functional>

namespace seec {

/// Defines the type of an augmentation callback. The first parameter is the
/// type of augmentation (e.g. "concept"), and the second parameter is the
/// identifier (e.g. "pointers").
///
using AugmentationCallbackFn =
  std::function<UnicodeString (UnicodeString const &, UnicodeString const &)>;

/// \brief Handle all augmentation markers in a string.
/// If \c Augmenter is present, then each marker will be substituted with
/// whatever is returned from the \c Augmenter. Otherwise, each marker will
/// be erased.
///
UnicodeString augment(UnicodeString String, AugmentationCallbackFn Augmenter);

} // namespace seec

#endif // SEEC_ICU_AUGMENTER_HPP
