//===- lib/Trace/StreamState.cpp ------------------------------------------===//
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

#include "seec/Trace/StreamState.hpp"

#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace trace {

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              StreamState const &State)
{
  Out << "  @" << State.getAddress()
      << ": " << State.getFilename()
      << " (" << State.getMode() << ")\n";
  return Out;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              DIRState const &State)
{
  Out << "  @" << State.getAddress()
      << ": " << State.getDirname() << "\n";
  return Out;
}

} // namespace trace (in seec)

} // namespace seec
