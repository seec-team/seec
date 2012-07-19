#include "seec/ICU/Resources.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

std::unique_ptr<ResourceBundle> getResourceBundle(char const *Package,
                                                  Locale const &GetLocale) {
  UErrorCode Status = U_ZERO_ERROR;
  
  std::unique_ptr<ResourceBundle> Resource
    (new ResourceBundle(Package, GetLocale, Status));
  
  if (U_SUCCESS(Status)) {
    return Resource;
  }
  
  return nullptr;
}

} // namespace seec
