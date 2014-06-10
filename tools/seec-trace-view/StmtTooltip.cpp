//===- tools/seec-trace-view/StmtTooltip.cpp ------------------------------===//
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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/ClangEPV/ClangEPV.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

#include <wx/tipwin.h>

#include "RuntimeValueLookup.hpp"
#include "StmtTooltip.hpp"
#include "ValueFormat.hpp"

wxTipWindow *makeStmtTooltip(wxWindow *Parent,
                             clang::Stmt const * const Stmt,
                             seec::cm::FunctionState const &ActiveFunction,
                             wxCoord MaxLength,
                             wxRect &RectBound)
{
  wxString TipString;

  auto const &Process = ActiveFunction.getParent().getParent();
  auto const Value = ActiveFunction.getStmtValue(Stmt);

  if (Value) {
    TipString << seec::towxString(getPrettyStringForInline(*Value,
                                                           Process,
                                                           Stmt))
              << "\n";
  }

  // Add the type of the value.
  if (auto const E = llvm::dyn_cast<clang::Expr>(Stmt))
    TipString << E->getType().getAsString() << "\n";

  // Attempt to get a general explanation of the statement.
  auto const MaybeExplanation =
    seec::clang_epv::explain(Stmt,
                             RuntimeValueLookupForFunction{&ActiveFunction});

  if (MaybeExplanation.assigned(0)) {
    auto const &Explanation = MaybeExplanation.get<0>();
    if (TipString.size())
      TipString << "\n";
    TipString << seec::towxString(Explanation->getString()) << "\n";
  }

  // Get any runtime errors related to the Stmt.
  for (auto const &RuntimeError : ActiveFunction.getRuntimeErrors()) {
    if (RuntimeError.getStmt() != Stmt)
      continue;

    auto const MaybeDescription = RuntimeError.getDescription();
    if (MaybeDescription.assigned(0)) {
      auto const &Description = MaybeDescription.get<0>();
      if (TipString.size())
        TipString << "\n";
      TipString << seec::towxString(Description->getString());
    }
  }

  // Display the generated tooltip (if any).
  if (TipString.size())
    return new wxTipWindow(Parent, TipString, MaxLength, nullptr, &RectBound);

  return nullptr;
}
