//===- lib/Util/Resources.cpp ---------------------------------------------===//
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

#include "seec/Util/Resources.hpp"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Path.h"

namespace seec {

std::string getResourceDirectory(llvm::StringRef ExecutablePath)
{
  // Find the location of the ICU resources, which should be fixed relative
  // to our executable path.
  // For Bundles find: ../../Resources
  // Otherwise find:   ../lib/seec/resources

  llvm::SmallString<256> Path = ExecutablePath;

  // remove executable name, then remove "bin" or "MacOS" (for bundles)
  llvm::sys::path::remove_filename(Path);
  llvm::sys::path::remove_filename(Path);

  if (Path.str().endswith("Contents")) { // Bundle
    llvm::sys::path::remove_filename(Path); // remove "Contents"
    llvm::sys::path::append(Path, "Resources");
  }
  else {
    llvm::sys::path::append(Path, "lib", "seec", "resources");
  }

  return Path.str();
}

} // namespace seec
