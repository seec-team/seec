//===- tools/seec-trace-view/StreamStatePanel.hpp -------------------------===//
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

#ifndef SEEC_TRACE_VIEW_STREAMSTATEPANEL_HPP
#define SEEC_TRACE_VIEW_STREAMSTATEPANEL_HPP

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>


namespace seec {
  namespace cm {
    class ProcessState;
    class ThreadState;
  }
}

class ActionRecord;
class ActionReplayFrame;
class ContextNotifier;
class StateAccessToken;
class StreamPanel;
class wxBookCtrlBase;


/// \brief Displays a collection of state viewers.
///
class StreamStatePanel final : public wxPanel
{
  /// Holds the individual stream pages.
  wxBookCtrlBase *Book;

  /// Lookup pages by FILE * value.
  std::map<uintptr_t, StreamPanel *> Pages;

  /// The central handler for context notifications.
  ContextNotifier *Notifier;

  /// Used to record user interactions.
  ActionRecord *Recording;

  /// Token for accessing the current state.
  std::shared_ptr<StateAccessToken> CurrentAccess;

public:
  /// \brief Construct.
  ///
  StreamStatePanel();

  /// \brief Construct and create.
  ///
  StreamStatePanel(wxWindow *Parent,
                   ContextNotifier &WithNotifier,
                   ActionRecord &WithRecording,
                   ActionReplayFrame &WithReplay,
                   wxWindowID ID = wxID_ANY,
                   wxPoint const &Position = wxDefaultPosition,
                   wxSize const &Size = wxDefaultSize);

  /// \brief Create (if default constructed).
  ///
  bool Create(wxWindow *Parent,
              ContextNotifier &WithNotifier,
              ActionRecord &WithRecording,
              ActionReplayFrame &WithReplay,
              wxWindowID ID = wxID_ANY,
              wxPoint const &Position = wxDefaultPosition,
              wxSize const &Size = wxDefaultSize);

  /// \brief Update this panel to reflect the given state.
  ///
  void show(std::shared_ptr<StateAccessToken> Access,
            seec::cm::ProcessState const &Process,
            seec::cm::ThreadState const &Thread);

  /// \brief Clear the display of this panel.
  ///
  void clear();
};

#endif // SEEC_TRACE_VIEW_STREAMSTATEPANEL_HPP
