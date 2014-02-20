//===- lib/Clang/MappedRuntimeErrorState.cpp ------------------------------===//
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

#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedRuntimeErrorState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Util/Printing.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"

#include "llvm/Support/raw_ostream.h"

#include "unicode/locid.h"

#include <cassert>


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// RuntimeErrorState
//===----------------------------------------------------------------------===//

RuntimeErrorState::
RuntimeErrorState(FunctionState &WithParent,
                  seec::trace::RuntimeErrorState const &ForUnmappedState)
: Parent(WithParent),
  UnmappedState(ForUnmappedState)
{}

void RuntimeErrorState::print(llvm::raw_ostream &Out,
                              seec::util::IndentationGuide &Indentation) const
{
  auto const MaybeDescription = getDescription();
  assert(MaybeDescription.assigned());
  
  if (MaybeDescription.assigned(0)) {
    Out << Indentation.getString()
        << MaybeDescription.get<0>()->getString() << "\n";
  }
  else if (MaybeDescription.assigned<seec::Error>()){
    UErrorCode Status = U_ZERO_ERROR;
    auto Str = MaybeDescription.get<seec::Error>().getMessage(Status, Locale());
    if (U_SUCCESS(Status))
      Out << Indentation.getString() << Str << "\n";
  }
  
  auto const Statement = getStmt();
  if (!Statement)
    return;
  
  clang::LangOptions LangOpts;
  clang::PrintingPolicy Policy(LangOpts);
  auto const Indent = Indentation.getString().length();
  
  Out << Indentation.getString();
  Statement->printPretty(Out, nullptr, Policy, Indent);
}

seec::runtime_errors::RunError const &RuntimeErrorState::getRunError() const {
  return UnmappedState.getRunError();
}

seec::Maybe<std::unique_ptr<seec::runtime_errors::Description>, seec::Error>
RuntimeErrorState::getDescription() const {
  return seec::runtime_errors::Description::create(getRunError());
}

clang::Stmt const *RuntimeErrorState::getStmt() const {
  auto const Instruction = UnmappedState.getInstruction();
  
  auto const &Trace = Parent.getParent().getParent().getProcessTrace();
  auto const &MappedModule = Trace.getMapping();
  auto const &MappedInst = MappedModule.getMapping(Instruction);
  
  return MappedInst.getStmt();
}

clang::Expr const *
RuntimeErrorState::
getParameter(seec::runtime_errors::ArgParameter const &Param) const {
  auto const BaseStmt = getStmt();
  if (!BaseStmt)
    return nullptr;
  
  auto const Call = llvm::dyn_cast<clang::CallExpr>(BaseStmt);
  if (!Call)
    return nullptr;
  
  auto const Index = Param.data();
  if (Index >= Call->getNumArgs())
    return nullptr;
  
  return Call->getArg(Index);
}

bool RuntimeErrorState::isActive() const {
  return UnmappedState.isActive();
}


//===----------------------------------------------------------------------===//
// llvm::raw_ostream output
//===----------------------------------------------------------------------===//

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              RuntimeErrorState const &State)
{
  seec::util::IndentationGuide Indent("  ");
  State.print(Out, Indent);
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
