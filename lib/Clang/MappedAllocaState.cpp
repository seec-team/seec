//===- lib/Clang/MappedAllocaState.cpp ------------------------------------===//
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


#include "seec/Clang/MappedAllocaState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Util/Printing.hpp"

#include "clang/AST/Decl.h"

#include "llvm/Support/raw_ostream.h"

#include <cassert>


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// AllocaState
//===----------------------------------------------------------------------===//

AllocaState::AllocaState(FunctionState &WithParent,
                         seec::trace::AllocaState &ForUnmappedState,
                         ::clang::VarDecl const *ForDecl)
: Parent(WithParent),
  UnmappedState(ForUnmappedState),
  Decl(ForDecl)
{
  assert(ForDecl && "Constructing AllocaState with null VarDecl.");
}

AllocaState::~AllocaState()
{}

void AllocaState::print(llvm::raw_ostream &Out,
                        seec::util::IndentationGuide &Indentation) const
{
  Out << Indentation.getString() << Decl->getName() << " = ";
  
  auto const Value = getValue();
  if (Value)
    Out << Value->getValueAsStringShort();
  else
    Out << "<unknown>";
  
  Out << "\n";
}

std::shared_ptr<Value const> AllocaState::getValue() const {
  auto const &ProcessState = Parent.getParent().getParent();
  
  auto const &Mapping = ProcessState.getProcessTrace().getMapping();
  auto const MappedAST = Mapping.getASTForDecl(Decl);
  assert(MappedAST && "Couldn't find AST for mapped Decl.");
  
  auto const &ASTContext = MappedAST->getASTUnit().getASTContext();
  return seec::cm::getValue(ProcessState.getCurrentValueStore(),
                            Decl->getType(),
                            ASTContext,
                            UnmappedState.getAddress(),
                            ProcessState.getUnmappedProcessState());
}


} // namespace cm (in seec)

} // namespace seec
