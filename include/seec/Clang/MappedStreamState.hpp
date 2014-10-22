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
  class DIRState;
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

  /// \brief Get the base (unmapped) state.
  ///
  seec::trace::StreamState const &getUnmappedState() const {
    return UnmappedState;
  }

  /// \brief Get the runtime address of the stream.
  ///
  uintptr_t getAddress() const;
  
  /// \brief Get the filename used when opening the stream.
  ///
  std::string const &getFilename() const;
  
  /// \brief Get the mode used when opening the stream.
  ///
  std::string const &getMode() const;
  
  /// \brief Get the data written to the stream so far.
  ///
  std::string const &getWritten() const;
  
  /// @} (Accessors.)
  
  
  /// \name Queries
  /// @{
  
  /// \brief Provides information about a single write to a stream.
  ///
  struct StreamWrite {
    std::size_t Begin; ///< Length of stream before this write occurred.
    std::size_t End;   ///< Length of stream after this write occurred.
  };
  
  /// \brief Get information about the write covering a given position.
  ///
  StreamWrite getWriteAt(std::size_t const Position) const;

  /// \brief Get the number of writes to this stream.
  ///
  std::size_t getWriteCount() const;

  /// \brief Get the n-th write to this stream.
  ///
  StreamWrite getWrite(std::size_t const Index) const;
  
  /// @} (Queries)
};


/// \brief Print a textual description of a StreamState.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, StreamState const &State);


/// \brief State of an open DIR.
///
class DIRState {
  /// The base (unmapped) state.
  seec::trace::DIRState const &UnmappedState;
  
public:
  /// \brief Constructor.
  ///
  DIRState(seec::trace::DIRState const &ForUnmappedState);
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the runtime address of the DIR.
  ///
  uintptr_t getAddress() const;
  
  /// \brief Get the pathname used when opening the DIR.
  ///
  std::string const &getDirname() const;
  
  /// @} (Accessors.)
};


/// \brief Print a textual description of a DIRState.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, DIRState const &State);

} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDSTREAMSTATE_HPP
