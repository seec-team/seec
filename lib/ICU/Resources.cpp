//===- lib/ICU/Resources.cpp ----------------------------------------------===//
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

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

std::unique_ptr<ResourceBundle> getResourceBundle(char const *Package,
                                                  Locale const &GetLocale)
{
  UErrorCode Status = U_ZERO_ERROR;

  std::unique_ptr<ResourceBundle> Resource
    (new ResourceBundle(Package, GetLocale, Status));

  if (U_SUCCESS(Status)) {
    return Resource;
  }

  return nullptr;
}

seec::Maybe<ResourceBundle, UErrorCode>
getResource(char const *Package, llvm::ArrayRef<char const *> const &Keys)
{
  UErrorCode Status = U_ZERO_ERROR;
  ResourceBundle Bundle(Package, Locale{}, Status);
  if (U_FAILURE(Status))
    return Status;
  return getResource(Bundle, Keys);
}

seec::Maybe<ResourceBundle, UErrorCode>
getResource(ResourceBundle const &RB, llvm::ArrayRef<char const *> const &Keys)
{
  UErrorCode Status = U_ZERO_ERROR;
  ResourceBundle Bundle(RB);
  
  if (U_FAILURE(Status))
    return Status;
  
  for (auto const &Key : Keys) {
    Bundle = Bundle.get(Key, Status);
    if (U_FAILURE(Status))
      return Status;
  }
  
  return Bundle;
}

seec::Maybe<UnicodeString, UErrorCode>
getString(char const *Package, llvm::ArrayRef<char const *> const &Keys)
{
  auto const MaybeBundle = getResource(Package, Keys);
  if (MaybeBundle.assigned<UErrorCode>())
    return MaybeBundle.get<UErrorCode>();
  
  UErrorCode Status = U_ZERO_ERROR;
  auto const &Bundle = MaybeBundle.get<ResourceBundle>();
  auto const String = Bundle.getString(Status);
  if (U_FAILURE(Status))
    return Status;
  
  return String;
}

seec::Maybe<UnicodeString, UErrorCode>
getString(ResourceBundle const &RB, llvm::ArrayRef<char const *> const &Keys)
{
  auto const MaybeBundle = getResource(RB, Keys);
  if (MaybeBundle.assigned<UErrorCode>())
    return MaybeBundle.get<UErrorCode>();
  
  UErrorCode Status = U_ZERO_ERROR;
  auto const &Bundle = MaybeBundle.get<ResourceBundle>();
  auto const String = Bundle.getString(Status);
  if (U_FAILURE(Status))
    return Status;
  
  return String;
}


//------------------------------------------------------------------------------
// ResourceLoader
//------------------------------------------------------------------------------

ResourceLoader::ResourceLoader(llvm::sys::Path const &ExecutablePath)
: ResourcesDirectory(ExecutablePath)
{
  // Find the location of the ICU resources, which should be fixed relative
  // to our executable path.
  // For Bundles find: ../../Resources
  // Otherwise find:   ../lib/seec/resources
  
  ResourcesDirectory.eraseComponent(); // remove executable name
  ResourcesDirectory.eraseComponent(); // remove "bin" or "MacOS" (bundle)
  
  if (llvm::StringRef(ResourcesDirectory.str()).endswith("Contents")) {
    ResourcesDirectory.eraseComponent(); // remove "Contents" (bundle)
    ResourcesDirectory.appendComponent("Resources");
  }
  else {
    ResourcesDirectory.appendComponent("lib");
    ResourcesDirectory.appendComponent("seec");
    ResourcesDirectory.appendComponent("resources");
  }
}

bool ResourceLoader::loadResource(char const *Package)
{
  std::string PackageStr (Package);

  // check if we've already loaded the package
  if (Resources.count(PackageStr))
    return true;

  // find and load the package
  auto PackagePath = ResourcesDirectory;
  PackagePath.appendComponent(Package);
  PackagePath.appendSuffix("dat");

  llvm::OwningPtr<llvm::MemoryBuffer> Holder;
  llvm::MemoryBuffer::getFile(PackagePath.str(), Holder);

  if (!Holder) {
    return false;
  }

  // add to our resource map
  auto Insert = Resources.insert(
                  std::make_pair(
                    std::move(PackageStr),
                    std::unique_ptr<llvm::MemoryBuffer>(Holder.take())));

  // register the data with ICU
  UErrorCode Status = U_ZERO_ERROR;

  udata_setAppData(Package, // package name
                   Insert.first->second->getBufferStart(), // data
                   &Status);

  if (U_FAILURE(Status)) {
    Resources.erase(Insert.first);
    return false;
  }

  return true;
}

} // namespace seec
