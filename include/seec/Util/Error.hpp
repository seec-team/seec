//===- seec/Util/Error.hpp ------------------------------------------ C++ -===//
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

#ifndef SEEC_UTIL_ERROR_HPP
#define SEEC_UTIL_ERROR_HPP

#include "seec/ICU/LazyMessage.hpp"

#include <memory>


namespace llvm {
  class raw_ostream;
}

namespace seec {


/// \brief Represents a general error that can be described with an
///        internationalized message.
///
class Error {
private:
  /// The description of this error.
  std::unique_ptr<seec::LazyMessage> Message;
  
public:
  /// \brief Create a new \c Error with the given description.
  /// \param WithMessage the description of this error.
  ///
  Error(std::unique_ptr<seec::LazyMessage> WithMessage)
  : Message(std::move(WithMessage))
  {}

  // Copying denied.
  Error(Error const &) = delete;
  Error &operator=(Error const &) = delete;

  // Moving allowed.
  Error(Error &&) = default;
  Error &operator=(Error &&) = default;

  /// \brief Get this error's message in a given \c Locale.
  /// \param Status indicates the result of this method.
  /// \param GetLocale the \c Locale to use for this message.
  /// \return this error's message in the given \c Locale, or a bogus string
  ///         if the method failed.
  ///
  UnicodeString getMessage(UErrorCode &Status, Locale const &GetLocale) const {
    if (U_FAILURE(Status))
      return UnicodeString();

    if (!Message)
      return UnicodeString();

    UnicodeString Msg = Message->get(Status, GetLocale);
    if (U_FAILURE(Status)) {
      return UnicodeString::fromUTF8("Couldn't load error message: ")
             + Message->describe();
    }

    return Msg;
  }

  /// \brief Get a description of the message that would be returned by
  ///        \c getMessage().
  /// This can be used to provide some information in the event that
  /// \c getMessage() fails (e.g. the appropriate ICU resources haven't been
  /// loaded).
  /// \return a description of this error's message.
  ///
  UnicodeString describeMessage() const {
    return Message->describe();
  }
};


llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, Error const &Err);


} // namespace seec


#endif // SEEC_UTIL_ERROR_HPP
