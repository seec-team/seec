//===- include/seec/RuntimeErrors/UnicodeFormatter.hpp --------------------===//
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

#ifndef SEEC_RUNTIMEERRORS_UNICODEFORMATTER_HPP
#define SEEC_RUNTIMEERRORS_UNICODEFORMATTER_HPP

#include "seec/ICU/Augmenter.hpp"
#include "seec/ICU/Indexing.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "unicode/unistr.h"

namespace seec {

namespace runtime_errors {

class RunError;


/// \brief Represents a formatted textual description of a RunError.
///
class Description {
  /// Formatted error message.
  seec::icu::IndexedString Message;
  
  /// Descriptions for the additional (subservient) errors.
  std::vector<std::unique_ptr<Description>> Additional;
  
  /// \brief Constructor.
  Description(seec::icu::IndexedString WithMessage,
              std::vector<std::unique_ptr<Description>> WithAdditional)
  : Message(std::move(WithMessage)),
    Additional(std::move(WithAdditional))
  {}
  
public:
  /// \brief Attempt to create a Description of the given RunError.
  ///
  static seec::Maybe<std::unique_ptr<Description>, seec::Error>
  create(RunError const &Error, AugmentationCallbackFn Augmenter);
  
  /// \brief Get a textual description of the parent error.
  ///
  UnicodeString const &getString() const { return Message.getString(); }
  
  /// \brief Get the Description objects for the additional (subservient)
  ///        errors.
  ///
  decltype(Additional) const &getAdditional() const { return Additional; }
};


/// \brief Provides common functionality for printing Description objects.
///
class DescriptionPrinterUnicode {
  /// The root Description.
  std::unique_ptr<Description> TheDescription;
  
  /// String used to separate consecutive Description strings.
  UnicodeString Separator;
  
  /// String used to indent additional Description strings.
  UnicodeString Indentation;
  
  /// The result of combining the strings of the Description tree.
  UnicodeString CombinedString;
  
  /// \brief Append a Description (and its children) to the CombinedString.
  ///
  void appendDescription(Description const &Desc,
                         unsigned Indent);
  
public:
  /// \brief Constructor.
  ///
  /// \param WithDescription The Description to print. Must be non-null.
  /// \param WithSeparator String to place between consecutive Descriptions.
  /// \param WithIndentation String to indent additional Descriptions.
  ///
  DescriptionPrinterUnicode(std::unique_ptr<Description> WithDescription,
                            UnicodeString WithSeparator,
                            UnicodeString WithIndentation);
  
  /// \brief Get the root Description.
  ///
  Description const &getDescription() const { return *TheDescription; }
  
  /// \brief Get the combined string.
  ///
  UnicodeString const &getString() const { return CombinedString; }
};


// UnicodeString format(RunError const &RunErr);


} // namespace runtime_errors (in seec)

} // namespace seec

#endif // SEEC_RUNTIMEERRORS_UNICODEFORMATTER_HPP
