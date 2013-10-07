//===- tools/seec-trace-view/CommonMenus.hpp ------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_COMMONMENUS_HPP
#define SEEC_TRACE_VIEW_COMMONMENUS_HPP

#include <wx/menu.h>

#include <memory>
#include <utility>

class wxEvtHandler;

/// \brief Create the standard file menu.
///
std::pair<std::unique_ptr<wxMenu>, wxString> createFileMenu();

/// \brief Create the standard recording settings menu.
///
std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(wxEvtHandler &EvtHandler);

/// \brief Appends a wxMenu to a wxMenuBar using the menu's current title.
///
bool append(wxMenuBar *MenuBar,
            std::pair<std::unique_ptr<wxMenu>, wxString> MenuWithTitle);

#endif // SEEC_TRACE_VIEW_COMMONMENUS_HPP
