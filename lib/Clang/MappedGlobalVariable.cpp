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

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              GlobalVariable const &State)
{
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
