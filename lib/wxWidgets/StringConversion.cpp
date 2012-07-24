#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/raw_ostream.h"

#include <string>

namespace seec {

wxString towxString(UnicodeString icu) {
  std::string Buffer;
  icu.toUTF8String(Buffer);
  return wxString(Buffer.c_str(), wxConvUTF8);
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

} // namespace seec
