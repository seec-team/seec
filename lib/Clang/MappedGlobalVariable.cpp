//===- lib/Clang/MappedGlobalVariable.cpp ---------------------------------===//
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

#include "seec/Clang/MappedGlobalVariable.hpp"

#include "clang/AST/Decl.h"

#include "llvm/Support/raw_ostream.h"


namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


//===----------------------------------------------------------------------===//
// GlobalVariable
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// GlobalVariable: Printing
//===----------------------------------------------------------------------===//

// Documented in MappedGlobalVariable.hpp
//
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              GlobalVariable const &State)
{
  Out << State.getClangValueDecl()->getName();
  
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
