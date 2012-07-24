//===- include/seec/ICU/Resources.hpp ------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_ICU_RESOURCES_HPP
#define SEEC_ICU_RESOURCES_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/PathV1.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/raw_ostream.h"

#include "unicode/resbund.h"
#include "unicode/udata.h"
#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/locid.h"

#include <map>
#include <memory>
#include <string>

namespace seec {

/// \brief Get a dynamically-allocated ICU ResourceBundle.
///
/// The resource bundle for the Package must have been previously loaded,
/// preferably using ResourceLoader.
///
std::unique_ptr<ResourceBundle> getResourceBundle(char const *Package,
                                                  Locale const &GetLocale);


/// Base-case for getResource operating on a ResourceBundle.
inline ResourceBundle getResource(ResourceBundle const &Bundle,
                                  UErrorCode &Status) {
  return Bundle;
}

/// \brief Get the ICU ResourceBundle at a given position in the heirarchy.
/// This function gets Bundle's internal ResourceBundle for the first key in
/// Keys, then gets that ResourceBundle's internal ResourceBundle for the next
/// key in Keys, and so on until Keys has been exhausted, at which point it
/// returns the final internal ResourceBundle.
/// \tparam KeyTs
/// \param Bundle
/// \param Status
/// \param Keys
/// \return
template<typename KeyT, typename... KeyTs>
ResourceBundle getResource(ResourceBundle const &Bundle,
                           UErrorCode &Status,
                           KeyT Key,
                           KeyTs... Keys) {
  return getResource(Bundle.get(Key, Status),
                     Status,
                     std::forward<KeyTs>(Keys)...);
}

/// \brief Get the ICU ResourceBundle at a given position in the heirarchy.
/// This function opens the ResourceBundle for Package using GetLocale, then
/// gets the internal ResourceBundle returned by traversing the heirarchy using
/// getResource with Keys.
/// \tparam KeyTs
/// \param Package
/// \param GetLocale
/// \param Status
/// \param Keys
/// \return
template<typename... KeyTs>
ResourceBundle getResource(char const *Package,
                           Locale const &GetLocale,
                           UErrorCode &Status,
                           KeyTs... Keys) {
  return getResource(ResourceBundle(Package, GetLocale, Status),
                     Status,
                     std::forward<KeyTs>(Keys)...);
}


/// \brief Handle loading and registering resources for ICU.
class ResourceLoader {
  llvm::sys::Path ResourcesDirectory;

  std::map<std::string, std::unique_ptr<llvm::MemoryBuffer>> Resources;

public:
  ResourceLoader(llvm::sys::Path const &ExecutablePath);

  bool loadResource(char const *Package);

  bool freeResource(llvm::StringRef Package) {
    return Resources.erase(Package.str()) != 0;
  }

  void freeAllResources() {
    Resources.clear();
  }
};

}

#endif // SEEC_ICU_RESOURCES_HPP
