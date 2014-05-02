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


namespace clang {
  class Stmt;
} // namespace clang

class wxEvtHandler;
class wxWindow;

class ActionRecord;
class StateAccessToken;
class TraceViewerFrame;


/// \brief Bind a \c wxMenuItem to the given \c std::function.
///
void BindMenuItem(wxMenuItem *Item,
                  std::function<void (wxEvent &)> Handler);

/// \brief Create the standard file menu.
///
std::pair<std::unique_ptr<wxMenu>, wxString> createFileMenu();

/// \brief Create the standard recording menu.
///
std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(wxEvtHandler &EvtHandler);

/// \brief Create the recording menu for a trace viewer frame.
///
std::pair<std::unique_ptr<wxMenu>, wxString>
createRecordingMenu(TraceViewerFrame &Viewer);

/// \brief Appends a wxMenu to a wxMenuBar using the menu's current title.
///
bool append(wxMenuBar *MenuBar,
            std::pair<std::unique_ptr<wxMenu>, wxString> MenuWithTitle);

/// \brief Add contextual navigation menu items for the given Stmt.
///
/// Movement occurs in the given thread.
///
void addStmtNavigation(wxWindow &Control,
                       std::shared_ptr<StateAccessToken> &Access,
                       wxMenu &Menu,
                       std::size_t const ThreadIndex,
                       clang::Stmt const *Statement,
                       ActionRecord * const Recording);


#endif // SEEC_TRACE_VIEW_COMMONMENUS_HPP
