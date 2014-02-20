//===- include/seec/ICU/Resources.hpp ------------------------------- C++ -===//
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

#ifndef SEEC_ICU_RESOURCES_HPP
#define SEEC_ICU_RESOURCES_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/system_error.h"

#include "unicode/resbund.h"
#include "unicode/udata.h"
#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/locid.h"

#include <cstdint>
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


/// \brief Base-case for getResource operating on a ResourceBundle.
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

/// \brief Get the ICU ResourceBundle at a given position in the heirarchy of
/// the named Package.
///
seec::Maybe<ResourceBundle, UErrorCode>
getResource(char const *Package, llvm::ArrayRef<char const *> const &Keys);

/// \brief Get the ICU ResourceBundle at a given position in the heirarchy
/// relative to the supplied ResourceBundle.
///
seec::Maybe<ResourceBundle, UErrorCode>
getResource(ResourceBundle const &RB, llvm::ArrayRef<char const *> const &Keys);

/// \brief Get the ICU UnicodeString at a given position in the heirarchy of
/// the named Package.
///
seec::Maybe<UnicodeString, UErrorCode>
getString(char const *Package, llvm::ArrayRef<char const *> const &Keys);

/// \brief Get the ICU UnicodeString at a given position in the heirarchy
/// relative to the supplied ResourceBundle.
///
seec::Maybe<UnicodeString, UErrorCode>
getString(ResourceBundle const &RB, llvm::ArrayRef<char const *> const &Keys);

/// \brief Returns a signed integer in a resource that has a given key.
///
/// This is analagous to getStringEx().
///
/// \param Bundle The resource bundle to extract from.
/// \param Key The key of the integer that will be extracted.
/// \param Status Fills in the outgoing error code.
inline int32_t getIntEx(ResourceBundle const &Bundle,
                        char const *Key,
                        UErrorCode &Status) {
  if (U_FAILURE(Status))
    return int32_t{};

  auto Resource = Bundle.get(Key, Status);
  if (U_FAILURE(Status))
    return int32_t{};

  return Resource.getInt(Status);
}

/// \brief Returns the binary data in a resource.
///
/// \param Bundle The resource bundle to extract from.
/// \param Status Fills in the outgoing error code.
inline llvm::ArrayRef<uint8_t> getBinary(ResourceBundle const &Resource,
                                         UErrorCode  &Status)
{
  int32_t Length = -1;
  auto Data = Resource.getBinary(Length, Status);
  if (U_FAILURE(Status) || Length < 0)
    return llvm::ArrayRef<uint8_t>();

  return llvm::ArrayRef<uint8_t>(Data, static_cast<std::size_t>(Length));
}

/// \brief Returns the binary data in a resource that has a given key.
///
/// \param Bundle The resource bundle to extract from.
/// \param Key The key of the binary data that will be extracted.
/// \param Status Fills in the outgoing error code.
inline llvm::ArrayRef<uint8_t> getBinaryEx(ResourceBundle const &Bundle,
                                           char const *Key,
                                           UErrorCode  &Status)
{
  return getBinary(Bundle.get(Key, Status), Status);
}


/// \brief Handle loading and registering resources for ICU.
///
class ResourceLoader {
  llvm::SmallString<256> ResourcesDirectory;

  std::map<std::string, std::unique_ptr<llvm::MemoryBuffer>> Resources;

public:
  ResourceLoader(llvm::StringRef ExecutablePath);
  
  llvm::StringRef getResourcesDirectory() const {
    return ResourcesDirectory.str();
  }

  bool loadResource(char const *Package);
  
  template<typename RangeT>
  bool loadResources(RangeT const &Resources) {
    for (auto const &Resource : Resources)
      if (!loadResource(Resource))
        return false;
    return true;
  }

  bool freeResource(llvm::StringRef Package) {
    return Resources.erase(Package.str()) != 0;
  }

  void freeAllResources() {
    Resources.clear();
  }
};

}

#endif // SEEC_ICU_RESOURCES_HPP
