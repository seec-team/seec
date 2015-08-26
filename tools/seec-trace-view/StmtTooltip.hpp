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
  class Decl;
  class Stmt;
}

class OpenTrace;
class wxTipWindow;


/// \brief Create a tooltip describing a \c clang::Decl.
///
wxTipWindow *makeDeclTooltip(wxWindow *Parent,
                             OpenTrace &Trace,
                             clang::Decl const * const Decl,
                             wxCoord MaxLength,
                             wxRect &RectBound);

/// \brief Create a tooltip describing a \c clang::Stmt, in the context of a
///        particular \c seec::cm::FunctionState.
///
wxTipWindow *makeStmtTooltip(wxWindow *Parent,
                             OpenTrace &Trace,
                             clang::Stmt const * const Stmt,
                             seec::cm::FunctionState const &ActiveFunction,
                             wxCoord MaxLength,
                             wxRect &RectBound);

/// \brief Create a tooltip describing a \c clang::Stmt.
///
wxTipWindow *makeStmtTooltip(wxWindow *Parent,
                             OpenTrace &Trace,
                             clang::Stmt const * const Stmt,
                             wxCoord MaxLength,
                             wxRect &RectBound);

#endif // SEEC_TRACE_VIEW_STMTTOOLTIP_HPP
