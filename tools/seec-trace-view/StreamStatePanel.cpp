//===- tools/seec-trace-view/StreamStatePanel.cpp -------------------------===//
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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedStreamState.hpp"

#include <wx/listbook.h>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "StateAccessToken.hpp"
#include "StreamStatePanel.hpp"


//===----------------------------------------------------------------------===//
// StreamPanel
//===----------------------------------------------------------------------===//

/// \brief Shows the contents of a single FILE stream.
///
class StreamPanel final : public wxPanel
{
  /// Displays the data written to this FILE.
  wxTextCtrl *Text;

  /// Pointer to the \c StreamState displayed by this \c StreamPanel.
  seec::cm::StreamState const *State;

  /// \brief Updated the display using our current \c State.
  ///
  void update() {
    Text->SetValue(wxString{State->getWritten()});
  }

public:
  /// \brief Construct a new \c StreamPanel for a given \c StreamState.
  ///
  StreamPanel(wxWindow *Parent,
              seec::cm::StreamState const &WithState)
  : wxPanel(Parent),
    Text(nullptr),
    State(&WithState)
  {
    Text = new wxTextCtrl(this,
                          wxID_ANY,
                          wxEmptyString,
                          wxDefaultPosition,
                          wxDefaultSize,
                          wxTE_MULTILINE | wxTE_READONLY);

    auto const Sizer = new wxBoxSizer(wxHORIZONTAL);
    Sizer->Add(Text, wxSizerFlags().Proportion(1).Expand());
    SetSizerAndFit(Sizer);

    update();
  }

  /// \brief Update our \c StreamState.
  ///
  void update(seec::cm::StreamState const &WithState)
  {
    State = &WithState;
    update();
  }
};

//===----------------------------------------------------------------------===//
// StreamStatePanel
//===----------------------------------------------------------------------===//

StreamStatePanel::StreamStatePanel()
: Book(nullptr),
  Pages(),
  Notifier(nullptr),
  Recording(nullptr),
  CurrentAccess(),
  CurrentProcess()
{}

StreamStatePanel::StreamStatePanel(wxWindow *Parent,
                                   ContextNotifier &WithNotifier,
                                   ActionRecord &WithRecording,
                                   ActionReplayFrame &WithReplay,
                                   wxWindowID ID,
                                   wxPoint const &Position,
                                   wxSize const &Size)
: StreamStatePanel()
{
  Create(Parent, WithNotifier, WithRecording, WithReplay, ID, Position, Size);
}

bool StreamStatePanel::Create(wxWindow *Parent,
                              ContextNotifier &WithNotifier,
                              ActionRecord &WithRecording,
                              ActionReplayFrame &WithReplay,
                              wxWindowID const ID,
                              wxPoint const &Position,
                              wxSize const &Size)
{
  if (!wxPanel::Create(Parent, ID, Position, Size))
    return false;

  Notifier = &WithNotifier;
  Recording = &WithRecording;

  Book = new wxListbook(this, ID, Position, Size);

  auto const Sizer = new wxBoxSizer(wxHORIZONTAL);
  Sizer->Add(Book, wxSizerFlags().Proportion(1).Expand());
  SetSizerAndFit(Sizer);

  return true;
}

void StreamStatePanel::show(std::shared_ptr<StateAccessToken> Access,
                            seec::cm::ProcessState const &Process,
                            seec::cm::ThreadState const &Thread)
{
  // Remove pages that no longer exist, update those that do.
  for (auto It = Pages.begin(), End = Pages.end(); It != End; ++It) {
    if (auto const StreamPtr = Process.getStream(It->first)) {
      It->second->update(*StreamPtr);
    }
    else {
      Pages.erase(It);
    }
  }

  for (auto const &StreamEntry : Process.getStreams()) {
    auto const Address = StreamEntry.first;
    auto const It = Pages.lower_bound(Address);

    // If this FILE doesn't have a page then create one.
    if (It == Pages.end() || It->first != Address) {
      auto const StreamPage = new StreamPanel(this, StreamEntry.second);
      Book->AddPage(StreamPage, wxString{StreamEntry.second.getFilename()});
      Pages.insert(It, std::make_pair(Address, StreamPage));
    }
  }
}

void StreamStatePanel::clear()
{
  Book->DeleteAllPages();
  Pages.clear();
}
