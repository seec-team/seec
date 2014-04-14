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
    [&, ThreadIndex, Statement] (wxEvent &Ev) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> bool {
          auto &Thread = State.getThread(ThreadIndex);
          return seec::cm::moveBackwardUntilEvaluated(Thread, Statement);
        });
    });
  
  BindMenuItem(
    Menu.Append(wxID_ANY,
                seec::getwxStringExOrEmpty(TextTable, "StmtForward")),
    [&, ThreadIndex, Statement] (wxEvent &Ev) -> void {
      raiseMovementEvent(Control, Access,
        [=] (seec::cm::ProcessState &State) -> bool {
          auto &Thread = State.getThread(ThreadIndex);
          return seec::cm::moveForwardUntilEvaluated(Thread, Statement);
        });
    });
}
