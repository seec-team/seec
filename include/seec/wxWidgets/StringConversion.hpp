//===- include/seec/wxWidgets/StringConversion.hpp ------------------ C++ -===//
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

#ifndef SEEC_WXWIDGETS_STRINGCONVERSION_HPP
#define SEEC_WXWIDGETS_STRINGCONVERSION_HPP

#include "llvm/ADT/ArrayRef.h"

#include <unicode/unistr.h>
#include <unicode/resbund.h>

#include <wx/string.h>

namespace seec {

class Error;

/// \brief Convert a UnicodeString into a wxString.
wxString towxString(UnicodeString const &icu);

/// \brief Convert a wxString into a UnicodeString.
UnicodeString toUnicodeString(wxString const &wx);

/// \brief Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
///
wxString getwxStringEx(ResourceBundle const &Bundle,
                       char const *Key,
                       UErrorCode &Status);

/// \brief Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
///
/// If the extraction fails, return the given default.
///
wxString getwxStringExOr(ResourceBundle const &Bundle,
                         char const *Key,
                         wxString const &Default = wxEmptyString);

/// \brief Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
/// 
/// If the extraction fails, terminate the program.
///
wxString getwxStringExOrDie(ResourceBundle const &Bundle,
                            char const *Key);

/// \brief Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
///
/// If the extraction fails, return an empty string.
///
wxString getwxStringExOrEmpty(ResourceBundle const &Bundle,
                              char const *Key);

/// \brief Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
///
/// If the extraction fails, return a string containing the Key.
///
wxString getwxStringExOrKey(ResourceBundle const &Bundle,
                            char const *Key);

/// \brief Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
///
/// If the extraction fails, return an empty string.
///
wxString getwxStringExOrEmpty(ResourceBundle const &Bundle,
                              llvm::ArrayRef<char const *> const &Keys);

/// \brief Get a string using seec::getString(), then convert it to a wxString.
/// If the string extraction or conversion fails, return an empty string.
///
wxString getwxStringExOrEmpty(char const *Package,
                              llvm::ArrayRef<char const *> const &Keys);

/// \brief Get a \c seec::Error 's message or description as a \c wxString.
///
wxString getMessageOrDescribe(seec::Error const &Error,
                              Locale const &ForLocale);

} // namespace seec

#endif // SEEC_WXWIDGETS_STRINGCONVERSION_HPP
