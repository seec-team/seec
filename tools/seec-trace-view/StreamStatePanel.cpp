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
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/Clang/MappedStreamState.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/aui/auibook.h>
#include <wx/stc/stc.h>

#include "ActionRecord.hpp"
#include "ActionReplay.hpp"
#include "CommonMenus.hpp"
#include "ProcessMoveEvent.hpp"
#include "StateAccessToken.hpp"
#include "StreamStatePanel.hpp"
#include "SourceViewerSettings.hpp"


//===----------------------------------------------------------------------===//
// StreamPanel
//===----------------------------------------------------------------------===//

/// \brief Shows the contents of a single FILE stream.
///
class StreamPanel final : public wxPanel
{
  /// Displays the data written to this FILE.
  wxStyledTextCtrl *Text;

  /// Used to record user interactions.
  ActionRecord *Recording;

  /// Parent's token for accessing the current state.
  std::shared_ptr<StateAccessToken> &ParentAccess;

  /// Pointer to the \c StreamState displayed by this \c StreamPanel.
  seec::cm::StreamState const *State;

  /// Character that the mouse is currently hovering over.
  long MouseOverPosition;

  /// Start of hover highlight range.
  long HighlightStart;

  /// Length of hover highlight range.
  long HighlightLength;

  /// Used to determine if a right-click was performed without moving.
  bool ClickUnmoved;

  /// \brief Clear highlighting used for mouse hover.
  ///
  void clearHighlight() {
    if (HighlightLength) {
      Text->IndicatorClearRange(HighlightStart, HighlightLength);
      HighlightStart = 0;
      HighlightLength = 0;
    }
  }

  /// \brief Updated the display using our current \c State.
  ///
  void update() {
    clearHighlight();
    MouseOverPosition = wxSTC_INVALID_POSITION;
    ClickUnmoved = false;

    Text->SetReadOnly(false);
    Text->SetValue(wxString{State->getWritten()});
    Text->SetReadOnly(true);
    Text->ScrollToEnd();
  }

  void OnTextMotion(wxMouseEvent &Ev) {
    // When we exit this scope, call Event.Skip() so that it is handled by the
    // default handler.
    auto const SkipEvent = seec::scopeExit([&](){ Ev.Skip(); });

    // Clear this in case we are inbetween right down and right up.
    ClickUnmoved = false;

    // Find the character that is being hovered over.
    long Position;
    auto const Test = Text->HitTest(Ev.GetPosition(), &Position);

    if (Test != wxTE_HT_ON_TEXT
        || Position < 0
        || static_cast<unsigned long>(Position) >= State->getWritten().size()
        || Position == MouseOverPosition)
      return;

    clearHighlight();

    MouseOverPosition = Position;

    // Highlight the write that we are hovering over.
    auto const Write = State->getWriteAt(Position);
    HighlightStart = Write.Begin;
    HighlightLength = Write.End - Write.Begin;
    Text->IndicatorFillRange(HighlightStart, HighlightLength);
  }

  void OnTextEnter(wxMouseEvent &Ev) {
    if (Recording) {
      Recording->recordEventL("StreamPanel.MouseEnter",
                              make_attribute("address", State->getAddress()),
                              make_attribute("file", State->getFilename()));
    }
  }

  void OnTextLeave(wxMouseEvent &Ev) {
    if (Recording) {
      Recording->recordEventL("StreamPanel.MouseLeave",
                              make_attribute("address", State->getAddress()),
                              make_attribute("file", State->getFilename()));
    }

    MouseOverPosition = wxSTC_INVALID_POSITION;
    clearHighlight();
  }

  void OnRightDown(wxMouseEvent &Ev) {
    if (MouseOverPosition == wxSTC_INVALID_POSITION)
      return;
    ClickUnmoved = true;
    Ev.Skip();
  }

  void OnRightUp(wxMouseEvent &Ev) {
    if (!ClickUnmoved) {
      Ev.Skip();
      return;
    }

    // Save the mouse over position into a temporary so that we can capture it
    // by value (because it may change before the menu item is clicked).
    auto const Position = MouseOverPosition;

    wxMenu CM{};

    BindMenuItem(
      CM.Append(wxID_ANY,
                seec::getwxStringExOrEmpty("TraceViewer",
                  (char const *[]){"ContextualNavigation",
                                   "StreamRewindToWrite"})),
      [this, Position] (wxEvent &) -> void {
        if (Recording) {
          Recording->recordEventL(
            "ContextualNavigation.StreamRewindToWrite",
            make_attribute("address", State->getAddress()),
            make_attribute("file", State->getFilename()),
            make_attribute("position", Position));
        }

        raiseMovementEvent(*this, ParentAccess,
          [=] (seec::cm::ProcessState &ProcessState) -> bool {
            return seec::cm::moveBackwardToStreamWriteAt(ProcessState,
                                                         *State,
                                                         Position);
          });
      });

    PopupMenu(&CM);
  }

public:
  /// \brief Construct a new \c StreamPanel for a given \c StreamState.
  ///
  StreamPanel(wxWindow * const Parent,
              ActionRecord * const WithRecording,
              std::shared_ptr<StateAccessToken> &WithParentAccess,
              seec::cm::StreamState const &WithState)
  : wxPanel(Parent),
    Text(nullptr),
    Recording(WithRecording),
    ParentAccess(WithParentAccess),
    State(&WithState),
    MouseOverPosition(wxSTC_INVALID_POSITION),
    HighlightStart(0),
    HighlightLength(0),
    ClickUnmoved(false)
  {
    Text = new wxStyledTextCtrl(this, wxID_ANY);
    Text->SetReadOnly(true);

    setupAllSciCommonTypes(*Text);
    setupAllSciLexerTypes(*Text);

    // We only use one indicator (highlight), so set it here.
    setupAllSciIndicatorTypes(*Text);
    auto const Indicator = static_cast<int>(SciIndicatorType::CodeHighlight);
    Text->SetIndicatorCurrent(Indicator);

    Text->Bind(wxEVT_MOTION,       &StreamPanel::OnTextMotion, this);
    Text->Bind(wxEVT_ENTER_WINDOW, &StreamPanel::OnTextEnter, this);
    Text->Bind(wxEVT_LEAVE_WINDOW, &StreamPanel::OnTextLeave, this);
    Text->Bind(wxEVT_RIGHT_DOWN,   &StreamPanel::OnRightDown, this);
    Text->Bind(wxEVT_RIGHT_UP,     &StreamPanel::OnRightUp, this);

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
  CurrentAccess()
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

  Book = new wxAuiNotebook(this, ID, Position, Size,
                           wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT
                           | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS);

  auto const Sizer = new wxBoxSizer(wxHORIZONTAL);
  Sizer->Add(Book, wxSizerFlags().Proportion(1).Expand());
  SetSizerAndFit(Sizer);

  return true;
}

void StreamStatePanel::show(std::shared_ptr<StateAccessToken> Access,
                            seec::cm::ProcessState const &Process,
                            seec::cm::ThreadState const &Thread)
{
  CurrentAccess = std::move(Access);

  // Remove pages that no longer exist, update those that do.
  for (auto It = Pages.begin(), End = Pages.end(); It != End; ) {
    if (auto const StreamPtr = Process.getStream(It->first)) {
      It->second->update(*StreamPtr);
      ++It;
    }
    else {
      auto const Idx = Book->FindPage(It->second);
      if (Idx != wxNOT_FOUND)
        Book->DeletePage(static_cast<std::size_t>(Idx));
      Pages.erase(It++);
    }
  }

  for (auto const &StreamEntry : Process.getStreams()) {
    auto const Address = StreamEntry.first;
    auto const It = Pages.lower_bound(Address);
    
    // If this FILE doesn't have a page then create one. If it isn't stdin or
    // stdout, then make it the selected page. This selects newly opened (or
    // unclosed) streams as we move through the trace, which is nice, and also
    // ensures that stdout is selected when we first open a trace.
    if (It == Pages.end() || It->first != Address) {
      auto const &StreamName = StreamEntry.second.getFilename();
      auto const Select = StreamName != "stdin" && StreamName != "stderr";

      auto const StreamPage = new StreamPanel(this,
                                              this->Recording,
                                              CurrentAccess,
                                              StreamEntry.second);

      Book->InsertPage(0,
                       StreamPage,
                       wxString{StreamEntry.second.getFilename()},
                       Select);

      Pages.insert(It, std::make_pair(Address, StreamPage));
    }
  }
}

void StreamStatePanel::clear()
{
  Book->DeleteAllPages();
  Pages.clear();
}
