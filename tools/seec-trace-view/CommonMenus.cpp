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
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/menu.h>

#include "CommonMenus.hpp"
#include "ProcessMoveEvent.hpp"


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
