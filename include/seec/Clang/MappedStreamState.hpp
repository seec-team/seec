//===- include/seec/Clang/MappedStreamState.hpp --------------------- C++ -===//
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

#ifndef SEEC_CLANG_MAPPEDSTREAMSTATE_HPP
#define SEEC_CLANG_MAPPEDSTREAMSTATE_HPP


#include <string>


namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace trace {
  class StreamState;
} // namespace trace (in seec)

namespace cm {

/// \brief State of an open FILE stream.
///
class StreamState {
  /// The base (unmapped) state.
  seec::trace::StreamState const &UnmappedState;
  
public:
  /// \brief Constructor.
  ///
  StreamState(seec::trace::StreamState const &ForUnmappedState);
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the runtime address of the stream.
  ///
  uintptr_t getAddress() const;
  
  /// \brief Get the filename used when opening the stream.
  ///
  std::string const &getFilename() const;
  
  /// \brief Get the mode used when opening the stream.
  ///
  std::string const &getMode() const;
  
  /// @} (Accessors.)
};


/// \brief Print a textual description of a StreamState.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, StreamState const &State);

} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDSTREAMSTATE_HPP
