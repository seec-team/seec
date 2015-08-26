//===- include/seec/Trace/TracePointer.hpp -------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACEPOINTER_HPP
#define SEEC_TRACE_TRACEPOINTER_HPP

#include <cstdint>


namespace llvm {
  class raw_ostream;
}


namespace seec {

namespace trace {

/// \brief Represents the target of a pointer.
///
class PointerTarget
{
  /// Base address of the referenced allocation.
  uintptr_t Base;

  /// Temporal identifier for the referenced allocation.
  uint64_t Time;

public:
  /// \brief Construct a new null \c PointerTarget.
  ///
  PointerTarget()
  : Base(0),
    Time(0)
  {}

  /// \brief Construct a new \c PointerTarget.
  ///
  PointerTarget(uintptr_t const WithBase, uint64_t const WithTime)
  : Base(WithBase),
    Time(WithTime)
  {}

  uintptr_t getBase() const { return Base; }

  uint64_t getTemporalID() const { return Time; }

  /// \brief Check if this \c PointerTarget is non-null.
  ///
  explicit operator bool() const { return Base != 0; }

  /// \brief Check if this \c PointerTarget equals \c RHS.
  ///
  bool operator==(PointerTarget const &RHS) const {
    return Base == RHS.Base && Time == RHS.Time;
  }
};

/// \brief Support writing \c PointerTarget to \c llvm::raw_ostream.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out, PointerTarget const &Obj);


} // namespace trace (in seec)

} // namespace seec


#endif // SEEC_TRACE_TRACEPOINTER_HPP
