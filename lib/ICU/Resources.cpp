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


//------------------------------------------------------------------------------
// ResourceLoader
//------------------------------------------------------------------------------

ResourceLoader::ResourceLoader(llvm::sys::Path const &ExecutablePath)
: ResourcesDirectory(ExecutablePath)
{
  if (!ResourcesDirectory.empty()) {
    // ResourcesDirectory should be */bin/
    // we want */lib/seec/resources/
    ResourcesDirectory.eraseComponent();
    ResourcesDirectory.eraseComponent();
    ResourcesDirectory.appendComponent("lib");
    ResourcesDirectory.appendComponent("seec");
    ResourcesDirectory.appendComponent("resources");
  }

  if (!ResourcesDirectory.canRead()) {
    // try default location (TODO: base this on build settings?)
    ResourcesDirectory.set("/usr/local/lib/seec/resources");

    if (!ResourcesDirectory.canRead()) {
      ResourcesDirectory.clear();
    }
  }
}

bool ResourceLoader::loadResource(char const *Package) {
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
