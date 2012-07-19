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

/// \brief Get an ICU ResourceBundle.
///
/// The resource bundle for the Package must have been previously loaded,
/// preferably using ResourceLoader.
///
std::unique_ptr<ResourceBundle> getResourceBundle(char const *Package,
                                                  Locale const &GetLocale);

/// \brief Handled loading and registering resources for ICU.
class ResourceLoader {
  llvm::sys::Path ResourcesDirectory;
  
  std::map<std::string, std::unique_ptr<llvm::MemoryBuffer>> Resources;
  
public:
  ResourceLoader(llvm::sys::Path const &ExecutablePath)
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
  
  bool loadResource(char const *Package) {
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
  
  bool freeResource(llvm::StringRef Package) {
    return Resources.erase(Package.str()) != 0;
  }
  
  void freeAllResources() {
    Resources.clear();
  }
};

}

#endif // SEEC_ICU_RESOURCES_HPP
