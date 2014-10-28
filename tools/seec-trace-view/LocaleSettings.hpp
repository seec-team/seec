//===- tools/seec-trace-view/LocaleSettings.hpp ---------------------------===//
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

#ifndef SEEC_TRACE_VIEW_LOCALESETTINGS_HPP
#define SEEC_TRACE_VIEW_LOCALESETTINGS_HPP

#include <unicode/locid.h>

/// \brief Show the locale settings dialog.
///
void showLocaleSettings();

/// \brief Get the \c Locale that should be used.
///
icu::Locale getLocale();

#endif // SEEC_TRACE_VIEW_LOCALESETTINGS_HPP
