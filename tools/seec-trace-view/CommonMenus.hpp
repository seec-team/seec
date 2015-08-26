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
#include <vector>


namespace seec {
  namespace cm {
    class ProcessState;
    class Value;
  } // namespace cm
} // namespace seec

namespace clang {
  class Decl;
  class Stmt;
} // namespace clang

class wxEvtHandler;
class wxWindow;

class ActionRecord;
class ActionReplayFrame;
class OpenTrace;
class StateAccessToken;
class TraceViewerFrame;


/// \brief Bind a \c wxMenuItem to the given \c std::function.
///
void BindMenuItem(wxMenuItem *Item,
                  std::function<void (wxEvent &)> Handler);

/// \brief Create the standard file menu, with listed additional items.
/// Supported additional items: \c wxID_SAVEAS.
///
std::pair<std::unique_ptr<wxMenu>, wxString>
createFileMenu(std::vector<wxStandardID> const &AdditionalIDs);

/// \brief Create the standard file menu.
///
std::pair<std::unique_ptr<wxMenu>, wxString> createFileMenu();

/// \brief Create the standard edit menu.
///
std::pair<std::unique_ptr<wxMenu>, wxString> createEditMenu();

/// \brief Create the edit menu for a \c TraceViewerFrame.
///
std::pair<std::unique_ptr<wxMenu>, wxString>
createEditMenu(TraceViewerFrame &TheFrame);

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

/// \brief Add annotation editing menu item for the given Decl.
///
void addDeclAnnotationEdit(wxMenu &Menu,
                           wxWindow *Parent,
                           OpenTrace &Trace,
                           clang::Decl const *Declaration);

/// \brief Add annotation editing menu item for the given Stmt.
///
void addStmtAnnotationEdit(wxMenu &Menu,
                           wxWindow *Parent,
                           OpenTrace &Trace,
                           clang::Stmt const *Statement);

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

/// \brief Add contextual navigation items for the given \c Value.
///
void addValueNavigation(wxWindow &Control,
                        std::shared_ptr<StateAccessToken> &Access,
                        wxMenu &Menu,
                        seec::cm::Value const &Value,
                        seec::cm::ProcessState const &State,
                        ActionRecord * const Recording);

/// \brief Register handlers to replay contextual navigation menu events.
///
void registerNavigationReplay(wxWindow &Control,
                              std::shared_ptr<StateAccessToken> &Access,
                              ActionReplayFrame &Replay);

#endif // SEEC_TRACE_VIEW_COMMONMENUS_HPP
