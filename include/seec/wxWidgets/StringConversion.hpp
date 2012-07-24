//===- include/seec/wxWidgets/StringConversion.hpp ------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_WXWIDGETS_STRINGCONVERSION_HPP
#define SEEC_WXWIDGETS_STRINGCONVERSION_HPP

#include <unicode/unistr.h>
#include <unicode/resbund.h>

#include <wx/string.h>

namespace seec {

/// Convert a UnicodeString into a wxString.
wxString towxString(UnicodeString icu);

/// Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString.
wxString getwxStringEx(ResourceBundle const &Bundle,
                       char const *Key,
                       UErrorCode &Status);

/// Extract a UnicodeString from a ResourceBundle and convert it into a
/// wxString. If the extraction fails, terminate the program.
wxString getwxStringExOrDie(ResourceBundle const &Bundle,
                            char const *Key);

} // namespace seec

#endif // SEEC_WXWIDGETS_STRINGCONVERSION_HPP
