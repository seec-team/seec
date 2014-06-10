//===- tools/seec-trace-view/StmtTooltip.hpp ------------------------------===//
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

#ifndef SEEC_TRACE_VIEW_STMTTOOLTIP_HPP
#define SEEC_TRACE_VIEW_STMTTOOLTIP_HPP

#include <wx/wx.h>

namespace seec {
  namespace cm {
    class FunctionState;
  }
}

namespace clang {
  class Stmt;
}

class wxTipWindow;


/// \brief
///
wxTipWindow *makeStmtTooltip(wxWindow *Parent,
                             clang::Stmt const * const Stmt,
                             seec::cm::FunctionState const &ActiveFunction,
                             wxCoord MaxLength,
                             wxRect &RectBound);

#endif // SEEC_TRACE_VIEW_STMTTOOLTIP_HPP
