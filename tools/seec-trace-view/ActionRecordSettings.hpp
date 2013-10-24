//===- tools/seec-trace-view/ActionRecordSettings.hpp ---------------------===//
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

#ifndef SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP
#define SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP

/// \brief Show the action recording settings dialog (modal).
///
void showActionRecordSettings();

/// \brief Get the user's token for action recording.
///
/// If the stored token is invalid, this will return an empty string.
///
wxString getActionRecordToken();

/// \brief Get the user-specified limit for recordings (in MiB).
///
long getActionRecordSizeLimit();

#endif // SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP
