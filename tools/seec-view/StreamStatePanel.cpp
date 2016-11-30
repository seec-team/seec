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
#include "ColourSchemeSettings.hpp"
#include "CommonMenus.hpp"
#include "ProcessMoveEvent.hpp"
#include "StateAccessToken.hpp"
#include "StreamStatePanel.hpp"
#include "SourceViewerSettings.hpp"
#include "TraceViewerApp.hpp"

#include <cassert>
#include <cctype>


//===----------------------------------------------------------------------===//
// StreamPanel
//===----------------------------------------------------------------------===//

static
std::pair<int, int> getPositionsForCharacterRange(wxStyledTextCtrl *STC,
                                                  int const Start,
                                                  int const End)
{
  assert(Start <= End);

  // Initially set the offset to the first valid offset preceding the
  // "whole character" index. This will always be less than the required offset
  // (because no encoding uses less than one byte per character).
  int StartPos = STC->PositionBefore(Start);

  // Find the "whole character" index of the initial position, use that to
  // determine how many characters away from the desired position we are, and
  // then iterate to the desired position.
  auto const StartGuessCount = STC->CountCharacters(0, StartPos);
  for (int i = 0; i < Start - StartGuessCount; ++i)
    StartPos = STC->PositionAfter(StartPos);

  // Get the EndPos by iterating from the StartPos.
  auto const Length = End - Start;
  int EndPos = StartPos;
  for (int i = 0; i < Length; ++i)
    EndPos = STC->PositionAfter(EndPos);

  return std::make_pair(StartPos, EndPos);
}

/// \brief Shows the contents of a single FILE stream.
///
class StreamPanel final : public wxPanel
{
  enum class EModeKind {
    TextASCII,
    BinASCIIAndOctal
  };

  /// Displays the data written to this FILE.
  wxStyledTextCtrl *Text;

  /// Used to record user interactions.
  ActionRecord *Recording;

  /// Parent's token for accessing the current state.
  std::shared_ptr<StateAccessToken> &ParentAccess;

  /// Pointer to the \c StreamState displayed by this \c StreamPanel.
  seec::cm::StreamState const *State;

  /// Mode currently being used to display the data.
  EModeKind Mode;

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

    auto const &Written = State->getWritten();
    wxString DisplayString;

    static char const * const FormattedASCII[] = {
      "\\0", "soh", "stx", "etx", "eot", "enq", "ack", "bel",  "bs", "\\t",
      "\\n",  "vt", "\\f", "\\r",  "so",  "si", "dle", "dc1", "dc2", "dc3",
      "dc4", "nak", "syn", "etb", "can",  "em", "sub", "esc",  "fs",  "gs",
       "rs",  "us",  "sp",   "!",  "\"",   "#",   "$",   "%",   "&",   "'",
        "(",   ")",   "*",   "+",   ",",   "-",   ".",   "/",   "0",   "1",
        "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",   ":",   ";",
        "<",   "=",   ">",   "?",   "@",   "A",   "B",   "C",   "D",   "E",
        "F",   "G",   "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",   "X",   "Y",
        "Z",   "[",   "\\",  "]",   "^",   "_",   "`",   "a",   "b",   "c",
        "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",   "m",
        "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "{",   "|",   "}",   "~", "del" };

    // Automatically pick the mode based on whether or not there are non-ASCII
    // values in any of the stream writes.
    auto const HasNonASCII = std::any_of(Written.cbegin(), Written.cend(),
                                         [] (unsigned char const Value) {
                                           return Value > 127;
                                         });

    Mode = HasNonASCII ? EModeKind::BinASCIIAndOctal : EModeKind::TextASCII;

    if (Mode == EModeKind::TextASCII) {
      for (auto const Ch : Written) {
        if (std::isprint(Ch) || Ch == '\n') {
          DisplayString.Append(Ch);
        }
        else if (std::iscntrl(Ch) && 0 <= Ch && Ch <= 31) {
          DisplayString.Append(wxUniChar(0x2400+Ch));
        }
        else {
          DisplayString.Append(wxUniChar(0xFFFD));
        }
      }
    }
    else if (Mode == EModeKind::BinASCIIAndOctal) {
      auto const NumWrites = State->getWriteCount();
      char Buffer[5];

      for (std::size_t i = 0; i < NumWrites; ++i) {
        auto const Write = State->getWrite(i);

        for (std::size_t j = Write.Begin; j < Write.End; ++j) {
          auto const Ch = static_cast<unsigned char>(Written[j]);

          if (Ch < 0200)
            snprintf(Buffer, sizeof(Buffer), "%4s", FormattedASCII[Ch]);
          else
            snprintf(Buffer, sizeof(Buffer), " %03hho", Ch);

          DisplayString.Append(Buffer);
        }

        DisplayString.Append("\n");
      }
    }

    Text->SetReadOnly(false);
    Text->SetValue(DisplayString);
    Text->SetReadOnly(true);
    Text->ScrollToEnd();
  }

  void SetPositionAndHighlightFromHit(long const HitPos)
  {
    switch (Mode)
    {
      case EModeKind::TextASCII:
      {
        auto const Position = Text->CountCharacters(0, HitPos);
        if (Position == MouseOverPosition
            || Position < 0
            || static_cast<unsigned>(Position) >= State->getWritten().size())
          return;

        MouseOverPosition = Position;

        auto const Write = State->getWriteAt(Position);
        auto const Range =
          getPositionsForCharacterRange(Text, Write.Begin, Write.End);

        clearHighlight();
        HighlightStart  = Range.first;
        HighlightLength = Range.second - Range.first;

        break;
      }

      case EModeKind::BinASCIIAndOctal:
      {
        auto const Line = Text->LineFromPosition(HitPos);
        if (Line < 0 || static_cast<std::size_t>(Line) > State->getWriteCount())
          return;

        auto const Position = static_cast<long>(State->getWrite(Line).Begin);
        if (Position == MouseOverPosition)
          return;

        MouseOverPosition = Position;

        clearHighlight();
        HighlightStart  = Text->FindColumn(Line, 0);
        HighlightLength = Text->GetLineEndPosition(Line) - HighlightStart;
      }
    }

    Text->IndicatorFillRange(HighlightStart, HighlightLength);
  }

  void OnTextMotion(wxMouseEvent &Ev) {
    // When we exit this scope, call Event.Skip() so that it is handled by the
    // default handler.
    auto const SkipEvent = seec::scopeExit([&](){ Ev.Skip(); });

    // Clear this in case we are inbetween right down and right up.
    ClickUnmoved = false;

    // Find the position that the mouse is hovering over. Note that this is the
    // position in Scintilla's internal representation of the string, not
    // necessarily the index of the character being hovered over.
    long HitPos;
    auto const Test = Text->HitTest(Ev.GetPosition(), &HitPos);
    if (Test != wxTE_HT_ON_TEXT)
      return;

    SetPositionAndHighlightFromHit(HitPos);
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
          [=] (seec::cm::ProcessState &ProcessState) {
            return seec::cm::moveBackwardToStreamWriteAt(ProcessState,
                                                         *State,
                                                         Position);
          });
      });

    PopupMenu(&CM);
  }

public:
  /// \brief Apply the \c ColourScheme to this \c StreamPanel.
  ///
  void updateColourScheme(ColourScheme const &Scheme)
  {
    setupStylesFromColourScheme(*Text, Scheme);
  }

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
    Mode(EModeKind::TextASCII),
    MouseOverPosition(wxSTC_INVALID_POSITION),
    HighlightStart(0),
    HighlightLength(0),
    ClickUnmoved(false)
  {
    Text = new wxStyledTextCtrl(this, wxID_ANY);
    Text->SetReadOnly(true);

    updateColourScheme(
      *(wxGetApp().getColourSchemeSettings().getColourScheme()));

    // We only use one indicator (highlight), so set it here.
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

  // Handle ColourSchemeSettings changes.
  wxGetApp().getColourSchemeSettings().addListener(
    [this] (ColourSchemeSettings const &Settings) {
      for (auto &Entry : Pages) {
        Entry.second->updateColourScheme(*Settings.getColourScheme());
      }
    });

  return true;
}

/// Workaround because wxAuiNotebook has a bug that breaks member FindPage.
///
static int FindPage(wxBookCtrlBase const *Book, wxWindow const *Page)
{
  for (std::size_t i = 0, Count = Book->GetPageCount(); i < Count; ++i)
    if (Book->GetPage(i) == Page)
      return static_cast<int>(i);

  return wxNOT_FOUND;
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
      auto const Idx = FindPage(Book, It->second);
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
