//===- tools/seec-trace-view/CommonMenus.cpp ------------------------------===//
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
#include "seec/Clang/MappedValue.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeFunction.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/menu.h>

#include "ActionRecord.hpp"
#include "ActionRecordSettings.hpp"
#include "ActionReplay.hpp"
#include "CommonMenus.hpp"
#include "ProcessMoveEvent.hpp"
#include "TraceViewerFrame.hpp"

void BindMenuItem(wxMenuItem *Item,
                  std::function<void (wxEvent &)> Handler)
{
  if (!Item)
    return;
  
  auto const Menu = Item->GetMenu();
  if (!Menu)
    return;
  
  Menu->Bind(wxEVT_MENU, Handler, Item->GetId());
}

std::pair<std::unique_ptr<wxMenu>, wxString> createFileMenu()
{
  auto const Title = seec::getwxStringExOrEmpty("TraceViewer",
                        (char const *[]) {"GUIText", "Menu_File"});
  
  auto Menu = seec::makeUnique<wxMenu>();
  
  Menu->Append(wxID_OPEN);
  Menu->Append(wxID_CLOSE);
  Menu->AppendSeparator();
  Menu->Append(wxID_EXIT);
  
  return std::make_pair(std::move(Menu), Title);
}

std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(wxEvtHandler &EvtHandler)
{
#ifdef SEEC_USER_ACTION_RECORDING
  auto const Title = seec::getwxStringExOrEmpty("TraceViewer",
                        (char const * []){"GUIText", "MenuRecord", "Title"});
  
  auto Menu = seec::makeUnique<wxMenu>();
  
  // Item for opening the recording settings menu.
  BindMenuItem(
    Menu->Append(wxID_ANY,
                 seec::getwxStringExOrEmpty("TraceViewer",
                    (char const * []){"GUIText", "MenuRecord", "Settings"})),
    [] (wxEvent &) {
      showActionRecordSettings();
    });
  
  return std::make_pair(std::move(Menu), Title);
#else
  return std::make_pair(std::unique_ptr<wxMenu>(), wxEmptyString);
#endif
}

std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(TraceViewerFrame &Viewer)
{
  return createRecordingMenu(static_cast<wxEvtHandler &>(Viewer));
}

bool append(wxMenuBar *MenuBar,
            std::pair<std::unique_ptr<wxMenu>, wxString> MenuWithTitle)
{
  if (!MenuWithTitle.first)
    return false;
  
  return MenuBar->Append(MenuWithTitle.first.release(),
                         MenuWithTitle.second);
}

void addStmtNavigation(wxWindow &Control,
                       std::shared_ptr<StateAccessToken> &Access,
                       wxMenu &Menu,
                       std::size_t const ThreadIndex,
                       clang::Stmt const *Statement,
                       ActionRecord * const Recording)
{
  UErrorCode Status = U_ZERO_ERROR;
  auto const TextTable = seec::getResource("TraceViewer",
                                           Locale::getDefault(),
                                           Status,
                                           "ContextualNavigation");
  if (U_FAILURE(Status))
    return;
  
  BindMenuItem(
    Menu.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "StmtRewind")),
    [&, ThreadIndex, Statement, Recording] (wxEvent &Ev) -> void {
      if (Recording) {
        Recording->recordEventL("ContextualNavigation.StmtRewind",
                                make_attribute("thread", ThreadIndex),
                                make_attribute("stmt", Statement));
      }
      else {
        wxLogDebug("no recording.");
      }

      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> bool {
          auto &Thread = State.getThread(ThreadIndex);
          return seec::cm::moveBackwardUntilEvaluated(Thread, Statement);
        });
    });
  
  BindMenuItem(
    Menu.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "StmtForward")),
    [&, ThreadIndex, Statement, Recording] (wxEvent &Ev) -> void {
      if (Recording) {
        Recording->recordEventL("ContextualNavigation.StmtForward",
                                make_attribute("thread", ThreadIndex),
                                make_attribute("stmt", Statement));
      }
      else {
        wxLogDebug("no recording.");
      }

      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> bool {
          auto &Thread = State.getThread(ThreadIndex);
          return seec::cm::moveForwardUntilEvaluated(Thread, Statement);
        });
    });
}

static void recordValueNavigation(char const * const Navigation,
                                  seec::cm::Value const &Value,
                                  ActionRecord * const Recording)
{
  if (!Recording)
    return;

  std::vector<std::unique_ptr<IAttributeReadOnly>> Attributes;

  Attributes.emplace_back(new_attribute("address", Value.getAddress()));
  Attributes.emplace_back(new_attribute("size", Value.getTypeSizeInChars()
                                                     .getQuantity()));
  Attributes.emplace_back(new_attribute("type", Value.getTypeAsString()));

  switch (Value.getKind()) {
    case seec::cm::Value::Kind::Basic:
      Attributes.emplace_back(new_attribute("kind", "Basic"));
      break;

    case seec::cm::Value::Kind::Scalar:
      Attributes.emplace_back(new_attribute("kind", "Scalar"));
      break;

    case seec::cm::Value::Kind::Array:
      Attributes.emplace_back(new_attribute("kind", "Array"));
      break;

    case seec::cm::Value::Kind::Record:
      Attributes.emplace_back(new_attribute("kind", "Record"));
      break;

    case seec::cm::Value::Kind::Pointer:
      Attributes.emplace_back(new_attribute("kind", "Pointer"));
      break;
  }

  std::vector<IAttributeReadOnly const *> AttributePtrs;
  for (auto const &Attr : Attributes)
    AttributePtrs.emplace_back(Attr.get());

  Recording->recordEventV(Navigation, AttributePtrs);
}

void addValueNavigation(wxWindow &Control,
                        std::shared_ptr<StateAccessToken> &Access,
                        wxMenu &Menu,
                        seec::cm::Value const &Value,
                        ActionRecord * const Recording)
{
  UErrorCode Status = U_ZERO_ERROR;
  auto const TextTable = seec::getResource("TraceViewer",
                                           Locale::getDefault(),
                                           Status,
                                           "ContextualNavigation");
  if (U_FAILURE(Status))
    return;

  // Contextual movement based on the Value's memory.
  if (Value.isInMemory()) {
    auto const Size = Value.getTypeSizeInChars().getQuantity();
    auto const Area = seec::MemoryArea(Value.getAddress(), Size);

    BindMenuItem(
      Menu.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "ValueRewindAllocation")),
      [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
        recordValueNavigation("ContextualNavigation.ValueRewindAllocation",
                              Value, Recording);

        raiseMovementEvent(Control, Access,
          [=, &Value] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveToAllocation(State, Area.start());
          });
      });

    BindMenuItem(
      Menu.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "ValueRewindModification")),
      [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
        recordValueNavigation("ContextualNavigation.ValueRewindModification",
                              Value, Recording);

        raiseMovementEvent(Control, Access,
          [=] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveBackwardUntilMemoryChanges(State, Area);
          });
      });

    BindMenuItem(
      Menu.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "ValueForwardModification")),
      [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
        recordValueNavigation("ContextualNavigation.ValueForwardModification",
                              Value, Recording);

        raiseMovementEvent(Control, Access,
          [=] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveForwardUntilMemoryChanges(State, Area);
          });
      });

    BindMenuItem(
      Menu.Append(wxID_ANY,
                  seec::getwxStringExOrEmpty(TextTable,
                                             "ValueForwardDeallocation")),
      [=, &Control, &Access, &Value] (wxEvent &Ev) -> void {
        recordValueNavigation("ContextualNavigation.ValueForwardDeallocation",
                              Value, Recording);

        raiseMovementEvent(Control, Access,
          [=, &Value] (seec::cm::ProcessState &State) -> bool {
            return seec::cm::moveToDeallocation(State, Area.start());
          });
      });
  }
}

void registerStmtNavigationReplay(wxWindow &Control,
                                  std::shared_ptr<StateAccessToken> &Access,
                                  ActionReplayFrame &Replay)
{
  Replay.RegisterHandler("ContextualNavigation.StmtRewind",
                         {{"thread", "stmt"}}, seec::make_function(
    [&] (std::size_t const ThreadIdx, clang::Stmt const * const Stmt) -> void {
      raiseMovementEvent(Control, Access,
        [ThreadIdx, Stmt] (seec::cm::ProcessState &State) -> bool {
          auto &Thread = State.getThread(ThreadIdx);
          return seec::cm::moveBackwardUntilEvaluated(Thread, Stmt);
        });
    }));

  Replay.RegisterHandler("ContextualNavigation.StmtForward",
                         {{"thread", "stmt"}}, seec::make_function(
    [&] (std::size_t const ThreadIdx, clang::Stmt const * const Stmt) -> void {
      raiseMovementEvent(Control, Access,
        [ThreadIdx, Stmt] (seec::cm::ProcessState &State) -> bool {
          auto &Thread = State.getThread(ThreadIdx);
          return seec::cm::moveForwardUntilEvaluated(Thread, Stmt);
        });
    }));
}
