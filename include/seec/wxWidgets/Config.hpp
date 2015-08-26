//===- include/seec/wxWidgets/Config.hpp ---------------------------- C++ -===//
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

#ifndef SEEC_WXWIDGETS_CONFIG_HPP
#define SEEC_WXWIDGETS_CONFIG_HPP

#include <string>

namespace seec {

/// \brief Setup a dummy wxAppConsole named "seec".
/// This enabled some wxWidgets functionality which depends on the running
/// wxApp, such as \c wxStandardPaths::GetUserLocalDataDir(). This function
/// automatically registers a shutdown function with \c std::atexit(), which
/// will shutdown the dummy wxAppConsole.
///
void setupDummyAppConsole();

bool setupCommonConfig();

std::string getUserLocalDataPath();

} // namespace seec

#endif // SEEC_WXWIDGETS_CONFIG_HPP
