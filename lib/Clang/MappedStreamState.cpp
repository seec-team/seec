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

stateptr_ty StreamState::getAddress() const {
  return UnmappedState.getAddress();
}

bool StreamState::isstd() const
{
  return UnmappedState.isstd();
}

bool StreamState::isstdin() const
{
  return UnmappedState.isstdin();
}

bool StreamState::isstdout() const
{
  return UnmappedState.isstdout();
}

bool StreamState::isstderr() const
{
  return UnmappedState.isstderr();
}

std::string const &StreamState::getFilename() const {
  return UnmappedState.getFilename();
}

std::string const &StreamState::getMode() const {
  return UnmappedState.getMode();
}

std::string const &StreamState::getWritten() const {
  return UnmappedState.getWritten();
}

StreamState::StreamWrite
StreamState::getWriteAt(std::size_t const Position) const
{
  auto const UnmappedWrite = UnmappedState.getWriteAt(Position);
  return StreamWrite{UnmappedWrite.Begin, UnmappedWrite.End};
}

std::size_t StreamState::getWriteCount() const
{
  return UnmappedState.getWriteCount();
}

StreamState::StreamWrite StreamState::getWrite(std::size_t const Index) const
{
  auto const UnmappedWrite = UnmappedState.getWrite(Index);
  return StreamWrite{UnmappedWrite.Begin, UnmappedWrite.End};
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, StreamState const &State)
{
  Out << "@" << State.getAddress()
      << ": " << State.getFilename()
      << " (" << State.getMode() << ")";
  
  return Out;
}

DIRState::DIRState(seec::trace::DIRState const &ForUnmappedState)
: UnmappedState(ForUnmappedState)
{}

stateptr_ty DIRState::getAddress() const {
  return UnmappedState.getAddress();
}

std::string const &DIRState::getDirname() const {
  return UnmappedState.getDirname();
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, DIRState const &State)
{
  Out << "@" << State.getAddress()
      << ": " << State.getDirname();
  
  return Out;
}

} // namespace cm (in seec)

} // namespace seec
