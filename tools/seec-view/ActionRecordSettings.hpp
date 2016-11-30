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

#include <memory>

class wxString;

/// \brief Show the action recording settings dialog (modal).
///
void showActionRecordSettings();

/// \brief Get the user's token for action recording.
///
/// If the stored token is invalid, this will return an empty string.
///
wxString getActionRecordToken();

/// \brief Check if the user has a valid token for action recording.
///
bool hasValidActionRecordToken();

/// \brief Get the user-specified limit for recordings (in MiB).
///
long getActionRecordSizeLimit();

/// \brief Get the user-specified limit for locally stored recordings (in MiB).
///
long getActionRecordStoreLimit();

// Forward-declare implementation of ActionRecordingSubmitter.
class ActionRecordingSubmitterImpl;

/// \brief Handles the submission of action recordings.
///
class ActionRecordingSubmitter final
{
  std::unique_ptr<ActionRecordingSubmitterImpl> pImpl;

public:
  /// \brief Constructor.
  ///
  ActionRecordingSubmitter();

  /// \brief Terminates any active submissions.
  ///
  ~ActionRecordingSubmitter();

  /// \brief Notify that a new recording is available to submit.
  ///
  void notifyOfNewRecording(wxString const &FullPath);
};

#endif // SEEC_TRACE_VIEW_ACTIONRECORDSETTINGS_HPP
