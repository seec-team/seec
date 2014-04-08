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

StreamState::StreamWrite
StreamState::getWriteAt(std::size_t const Position) const
{
  auto const It = std::find_if(Writes.begin(), Writes.end(),
    [=](Write const &W) { return Position < W.NewLength; });

  assert(It != Writes.end());

  return StreamWrite{
    It == Writes.begin() ? 0 : std::prev(It)->NewLength,
    It->NewLength
  };
}

void StreamState::write(llvm::ArrayRef<char> Data)
{
  Written.append(Data.data(), Data.size());
  Writes.emplace_back(Written.length());
}

void StreamState::unwrite(uint64_t const Size)
{
  assert(Written.size() >= Size);
  
  Written.resize(Written.size() - Size);
  Writes.pop_back();

  assert(Writes.empty() || Writes.back().NewLength == Written.length());
}

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
