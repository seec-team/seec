//===- lib/Clang/MappedStreamState.cpp ------------------------------ C++ -===//
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

#include "seec/Clang/MappedStreamState.hpp"
#include "seec/Trace/StreamState.hpp"

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {

StreamState::StreamState(seec::trace::StreamState const &ForUnmappedState)
: UnmappedState(ForUnmappedState)
{}

uintptr_t StreamState::getAddress() const {
  return UnmappedState.getAddress();
}

std::string const &StreamState::getFilename() const {
  return UnmappedState.getFilename();
}

std::string const &StreamState::getMode() const {
  return UnmappedState.getMode();
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, StreamState const &State)
{
  Out << "@" << State.getAddress()
      << ": " << State.getFilename()
      << " (" << State.getMode() << ")";
  
  return Out;
}

} // namespace cm (in seec)

} // namespace seec
