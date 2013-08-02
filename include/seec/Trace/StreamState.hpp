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

#include <string>

namespace llvm {
  class raw_ostream;
}

namespace seec {

namespace trace {


/// \brief State of an open FILE stream.
///
class StreamState {
  /// The runtime address of the stream (the raw value of the FILE *).
  uintptr_t Address;
  
  /// The filename used when opening the stream.
  std::string Filename;
  
  /// The mode used when opening the stream.
  std::string Mode;
  
public:
  /// \brief Constructor.
  ///
  StreamState(uintptr_t WithAddress,
              std::string WithFilename,
              std::string WithMode)
  : Address(WithAddress),
    Filename(WithFilename),
    Mode(WithMode)
  {}
  
  /// \name Accessors
  /// @{
  
  /// \brief Get the runtime address of the stream.
  ///
  uintptr_t getAddress() const { return Address; }
  
  /// \brief Get the filename used when opening the stream.
  ///
  std::string const &getFilename() const { return Filename; }
  
  /// \brief Get the mode used when opening the stream.
  ///
  std::string const &getMode() const { return Mode; }
  
  /// @} (Accessors)
};

/// Print a textual description of a StreamState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              StreamState const &State);


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_STREAMSTATE_HPP
