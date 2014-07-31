//===- lib/Trace/TracePointer.cpp -----------------------------------------===//
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

#include "seec/Trace/TracePointer.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace trace {

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, PointerTarget const &Obj)
{
  Out << "[" << Obj.getBase() << ", " << Obj.getTemporalID() << "]";
  return Out;
}

} // namespace trace (in seec)

} // namespace seec
