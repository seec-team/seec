//===- include/seec/Trace/ThreadState.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_STREAMSTATE_HPP
#define SEEC_TRACE_STREAMSTATE_HPP

#include "seec/Trace/StateCommon.hpp"

#include "llvm/ADT/ArrayRef.h"

#include <string>
#include <vector>

namespace llvm {
  class raw_ostream;
}

namespace seec {

namespace trace {


/// \brief State of an open FILE stream.
///
class StreamState {
public:
  /// \brief Used to identify standard streams.
  ///
  enum class StandardStreamKind { none, in, out, err };

private:
  /// \brief Describes an individual write to a stream.
  ///
  struct Write {
    std::size_t const NewLength;

    Write(std::size_t const WithNewLength)
    : NewLength(WithNewLength)
    {}
  };
  
  /// The runtime address of the stream (the raw value of the FILE *).
  stateptr_ty Address;
  
  /// Indicates if this is one of the standard streams.
  StandardStreamKind Kind;

  /// The filename used when opening the stream.
  std::string Filename;
  
  /// The mode used when opening the stream.
  std::string Mode;
  
  /// The data written to the stream.
  std::string Written;
  
  /// Track the end positions of individual writes.
  std::vector<Write> Writes;
  
public:
  /// \brief Constructor.
  ///
  StreamState(stateptr_ty WithAddress,
              StandardStreamKind WithKind,
              std::string WithFilename,
              std::string WithMode)
  : Address(WithAddress),
    Kind(WithKind),
    Filename(std::move(WithFilename)),
    Mode(std::move(WithMode)),
    Written(),
    Writes()
  {}

  // Copying denied.
  StreamState(StreamState const &) = delete;
  StreamState &operator=(StreamState const &) = delete;

  // Movement OK.
  StreamState(StreamState &&) = default;
  StreamState &operator=(StreamState &&) = default;
  
  
  /// \name Accessors
  /// @{
  
  /// \brief Get the runtime address of the stream.
  ///
  stateptr_ty getAddress() const { return Address; }

  /// \brief Check if this is one of the standard streams.
  ///
  bool isstd()    const { return Kind != StandardStreamKind::none; }
  bool isstdin()  const { return Kind == StandardStreamKind::in; }
  bool isstdout() const { return Kind == StandardStreamKind::out; }
  bool isstderr() const { return Kind == StandardStreamKind::err; }
  
  /// \brief Get the filename used when opening the stream.
  ///
  std::string const &getFilename() const { return Filename; }
  
  /// \brief Get the mode used when opening the stream.
  ///
  std::string const &getMode() const { return Mode; }
  
  /// \brief Get the data written to the stream so far.
  ///
  std::string const &getWritten() const { return Written; }
  
  /// @} (Accessors)
  
  
  /// \name Queries
  /// @{
  
  /// \brief Provides information about a single write to a stream.
  ///
  struct StreamWrite {
    std::size_t Begin; ///< Length of stream before this write occurred.
    std::size_t End;   ///< Length of stream after this write occurred.
  };
  
  /// \brief Get information about the write covering the given position.
  ///
  StreamWrite getWriteAt(std::size_t const Position) const;
  
  /// \brief Get the number of writes to this stream.
  ///
  std::size_t getWriteCount() const {
    return Writes.size();
  }

  /// \brief Get the n-th write to this stream.
  ///
  StreamWrite getWrite(std::size_t const Index) const;

  /// @} (Queries)
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Write the given bytes to the stream.
  ///
  void write(llvm::ArrayRef<char> Data);
  
  /// \brief Remove the given number of bytes from the end of the stream.
  ///
  void unwrite(uint64_t const Size);
  
  /// @} (Mutators)
};

/// Print a textual description of a StreamState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              StreamState const &State);


/// \brief State of an open DIR.
///
class DIRState {
  /// The runtime address of the DIR (the raw value of the DIR *).
  stateptr_ty const Address;
  
  /// The path used to open the DIR.
  std::string const Dirname;
  
public:
  /// \brief Constructor.
  ///
  DIRState(stateptr_ty WithAddress,
           std::string WithDirname)
  : Address(WithAddress),
    Dirname(std::move(WithDirname))
  {}
  
  /// \name Accessors
  /// @{
  
  /// \brief Get the runtime address of the DIR.
  ///
  stateptr_ty getAddress() const { return Address; }
  
  /// \brief Get the pathname used when opening the DIR.
  ///
  std::string const &getDirname() const { return Dirname; }
  
  /// @} (Accessors)
};

/// Print a textual description of a DIRState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              DIRState const &State);


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_STREAMSTATE_HPP
