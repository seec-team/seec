//===- include/seec/DSA/MemoryArea.hpp ------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_DSA_MEMORYAREA_HPP
#define SEEC_DSA_MEMORYAREA_HPP

#include "seec/DSA/Interval.hpp"

#include <cstdint>
#include <cstring>

namespace seec {


///
class MemoryArea : public Interval<uint64_t> {
public:
  /// Default constructor.
  MemoryArea()
  : Interval(Interval<uint64_t>::withStartEnd(0, 0))
  {}

  /// Initializing constructor.
  MemoryArea(uint64_t Address, uint64_t Length)
  : Interval(Interval<uint64_t>::withStartLength(Address, Length))
  {}

  /// Initializing constructor.
  MemoryArea(void const *Start, size_t Length)
  : Interval(
      Interval<uint64_t>::withStartLength(reinterpret_cast<uint64_t>(Start),
                                          static_cast<uint64_t>(Length)))
  {}

  /// Copy constructor.
  MemoryArea(MemoryArea const &Other) = default;

  /// Copy assignment.
  MemoryArea &operator=(MemoryArea const &RHS) = default;


  /// \name Accessors
  /// @{

  /// Get the address of the first byte in this area.
  uint64_t address() const { return start(); }

  /// Get the address of the last byte in this area.
  uint64_t lastAddress() const { return last(); }

  /// @}
  
  
  /// \name Queries
  /// @{
  
  bool operator==(MemoryArea const &RHS) const {
    return Interval<uint64_t>::operator==(RHS);
  }
  
  bool operator!=(MemoryArea const &RHS) const {
    return Interval<uint64_t>::operator==(RHS);
  }
  
  /// @}
};


} // namespace seec

#endif // SEEC_DSA_MEMORYAREA_HPP
