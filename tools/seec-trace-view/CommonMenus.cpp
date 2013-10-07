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

#include "seec/Util/MakeUnique.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include <wx/wx.h>
#include <wx/event.h>

#include "ActionRecordSettings.hpp"
#include "CommonMenus.hpp"

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
                      auto Frame = new ActionRecordSettingsFrame(nullptr);
                      Frame->Show(true);
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
