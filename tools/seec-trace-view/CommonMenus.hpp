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

#include <memory>


namespace clang {
  class Stmt;
} // namespace clang

class wxMenu;
class wxWindow;

class StateAccessToken;


void addStmtNavigation(wxWindow &Control,
                       std::shared_ptr<StateAccessToken> &Access,
                       wxMenu &Menu,
                       clang::Stmt const *Statement);

#endif // SEEC_TRACE_VIEW_COMMONMENUS_HPP
