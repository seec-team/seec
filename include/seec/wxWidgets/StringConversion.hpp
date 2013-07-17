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

#include <unicode/unistr.h>
#include <unicode/resbund.h>

#include <wx/string.h>

namespace seec {

/// \brief Convert a UnicodeString into a wxString.
wxString towxString(UnicodeString const &icu);

/// \brief Convert a wxString into a UnicodeString.
UnicodeString toUnicodeString(wxString const &wx);

/// Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
wxString getwxStringEx(ResourceBundle const &Bundle,
                       char const *Key,
                       UErrorCode &Status);

/// Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString. If the extraction fails, terminate the program.
wxString getwxStringExOrDie(ResourceBundle const &Bundle,
                            char const *Key);

/// Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString. If the extraction fails, return an empty wxString.
wxString getwxStringExOrEmpty(ResourceBundle const &Bundle,
                              char const *Key);

} // namespace seec

#endif // SEEC_WXWIDGETS_STRINGCONVERSION_HPP
