//===- include/seec/Util/Resources.hpp ------------------------------ C++ -===//
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

#ifndef SEEC_UTIL_RESOURCES_HPP
#define SEEC_UTIL_RESOURCES_HPP

#include "llvm/ADT/StringRef.h"

#include <string>

namespace seec {

/// \brief Get the path to SeeC's resources directory, based on the path to
///        one of SeeC's binaries.
///
std::string getResourceDirectory(llvm::StringRef ExecutablePath);

} // namespace seec

#endif // SEEC_UTIL_RESOURCES_HPP
