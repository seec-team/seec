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
#include "seec/ICU/Resources.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/menu.h>

#include "ActionRecordSettings.hpp"
#include "CommonMenus.hpp"
#include "ProcessMoveEvent.hpp"

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
  auto const Title = seec::getwxStringExOrEmpty("TraceViewer",
                        (char const * []){"GUIText", "MenuRecord", "Title"});
  
  auto Menu = seec::makeUnique<wxMenu>();
  
  auto SettingsItem =
    Menu->Append(wxID_ANY,
                 seec::getwxStringExOrEmpty("TraceViewer",
                    (char const * []){"GUIText", "MenuRecord", "Settings"}));
  
  EvtHandler.Bind(wxEVT_COMMAND_MENU_SELECTED,
                  std::function<void (wxCommandEvent &)>(
                    [] (wxCommandEvent &Event) {
                      showActionRecordSettings();
                    }
                  ),
                  SettingsItem->GetId());
  
  return std::make_pair(std::move(Menu), Title);
}

bool append(wxMenuBar *MenuBar,
            std::pair<std::unique_ptr<wxMenu>, wxString> MenuWithTitle)
{
  return MenuBar->Append(MenuWithTitle.first.release(),
                         MenuWithTitle.second);
}

static void BindMenuItem(wxMenuItem *Item,
                         std::function<void (wxEvent &)> Handler)
{
  if (!Item)
    return;
  
  auto const Menu = Item->GetMenu();
  if (!Menu)
    return;
  
  Menu->Bind(wxEVT_MENU, Handler, Item->GetId());
}

void addStmtNavigation(wxWindow &Control,
                       std::shared_ptr<StateAccessToken> &Access,
                       wxMenu &Menu,
                       clang::Stmt const *Statement)
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
    [&, Statement] (wxEvent &Ev) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> bool {
          if (State.getThreadCount() == 1) {
            auto &Thread = State.getThread(0);
            return seec::cm::moveBackwardUntilEvaluated(Thread, Statement);
          }
          else {
            wxLogDebug("Multithread rewind not yet implemented.");
            return false;
          }
        });
    });
  
  BindMenuItem(
    Menu.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "StmtForward")),
    [&, Statement] (wxEvent &Ev) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> bool {
          if (State.getThreadCount() == 1) {
            auto &Thread = State.getThread(0);
            return seec::cm::moveForwardUntilEvaluated(Thread, Statement);
          }
          else {
            wxLogDebug("Multithread rewind not yet implemented.");
            return false;
          }
        });
    });
}
