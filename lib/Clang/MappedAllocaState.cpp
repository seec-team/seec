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
#include "seec/Util/Printing.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// AllocaState
//===----------------------------------------------------------------------===//

AllocaState::AllocaState(FunctionState &WithParent,
                         seec::trace::AllocaState &ForUnmappedState,
                         clang::VarDecl const *Decl)
{
  
}

void AllocaState::print(llvm::raw_ostream &Out,
                        seec::util::IndentationGuide &Indentation) const
{
  Out << Indentation.getString() << "Alloca\n";
}


} // namespace cm (in seec)

} // namespace seec
