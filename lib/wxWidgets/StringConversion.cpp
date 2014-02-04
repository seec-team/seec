//===- lib/wxWidgets/StringConversion.cpp ---------------------------------===//
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

#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <string>

namespace seec {

wxString towxString(UnicodeString const &icu) {
  std::string Buffer;
  icu.toUTF8String(Buffer);
  return wxString(Buffer.c_str(), wxConvUTF8);
}

UnicodeString toUnicodeString(wxString const &wx) {
  return UnicodeString::fromUTF8(wx.utf8_str().data());
}

wxString getwxStringEx(ResourceBundle const &Bundle,
                       char const *Key,
                       UErrorCode &Status) {
  auto Str = Bundle.getStringEx(Key, Status);

  if (U_SUCCESS(Status)) {
    return towxString(Str);
  }

  return wxString{};
}

wxString getwxStringExOr(ResourceBundle const &Bundle,
                         char const *Key,
                         wxString const &Default)
{
  UErrorCode Status = U_ZERO_ERROR;

  auto Str = Bundle.getStringEx(Key, Status);

  if (U_FAILURE(Status)) {
    return Default;
  }

  return towxString(Str);
}

wxString getwxStringExOrDie(ResourceBundle const &Bundle,
                            char const *Key) {
  UErrorCode Status = U_ZERO_ERROR;

  auto Str = Bundle.getStringEx(Key, Status);

  if (U_FAILURE(Status)) {
    llvm::errs() << "Couldn't get string for '" << Key
                 << "' from '" << Bundle.getKey()
                 << "' in " << Bundle.getName() << "\n";
    std::exit(EXIT_FAILURE);
  }

  return towxString(Str);
}

wxString getwxStringExOrEmpty(ResourceBundle const &Bundle,
                              char const *Key)
{
  return getwxStringExOr(Bundle, Key, wxEmptyString);
}

wxString getwxStringExOrKey(ResourceBundle const &Bundle,
                            char const *Key)
{
  return getwxStringExOr(Bundle, Key, Key);
}

wxString getwxStringExOrEmpty(char const *Package,
                              llvm::ArrayRef<char const *> const &Keys)
{
  auto const MaybeStr = getString(Package, Keys);
  
  if (MaybeStr.assigned<UnicodeString>())
    return towxString(MaybeStr.get<UnicodeString>());
  
  return wxString{};
}

} // namespace seec
