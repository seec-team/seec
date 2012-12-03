//===- include/seec/DSA/MemoryArea.hpp ------------------------------ C++ -===//
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

#ifndef SEEC_DSA_MEMORYAREA_HPP
#define SEEC_DSA_MEMORYAREA_HPP

#include "seec/DSA/Interval.hpp"

#include <cstdint>
#include <cstring>

namespace seec {


enum class MemoryPermission : uint8_t {
  None,
  ReadOnly,
  WriteOnly,
  ReadWrite
};


///
class MemoryArea : public Interval<uintptr_t> {
  MemoryPermission Access;
  
public:
  /// Default constructor.
  MemoryArea()
  : Interval(Interval<uintptr_t>::withStartEnd(0, 0)),
    Access(MemoryPermission::ReadWrite)
  {}

  /// Initializing constructor.
  MemoryArea(uintptr_t Address, std::size_t Length)
  : Interval(Interval<uintptr_t>::withStartLength(Address, Length)),
    Access(MemoryPermission::ReadWrite)
  {}
  
  /// Initializing constructor.
  MemoryArea(uintptr_t Address, std::size_t Length, MemoryPermission Access)
  : Interval(Interval<uintptr_t>::withStartLength(Address, Length)),
    Access(Access)
  {}

  /// Initializing constructor.
  MemoryArea(void const *Start, std::size_t Length)
  : Interval(
      Interval<uintptr_t>::withStartLength(reinterpret_cast<uintptr_t>(Start),
                                           static_cast<uintptr_t>(Length))),
    Access(MemoryPermission::ReadWrite)
  {}
  
  /// Initializing constructor.
  MemoryArea(void const *Start, std::size_t Length, MemoryPermission Access)
  : Interval(
      Interval<uintptr_t>::withStartLength(reinterpret_cast<uintptr_t>(Start),
                                           static_cast<uintptr_t>(Length))),
    Access(Access)
  {}

  /// Copy constructor.
  MemoryArea(MemoryArea const &Other) = default;

  /// Copy assignment.
  MemoryArea &operator=(MemoryArea const &RHS) = default;


  /// \name Accessors
  /// @{

  /// Get the address of the first byte in this area.
  uintptr_t address() const { return start(); }

  /// Get the address of the last byte in this area.
  uintptr_t lastAddress() const { return last(); }
  
  /// Get the access permissions for this memory area.
  MemoryPermission getAccess() const { return Access; }

  /// @}
  
  
  /// \name Queries
  /// @{
  
  bool operator==(MemoryArea const &RHS) const {
    return Interval<uintptr_t>::operator==(RHS);
  }
  
  bool operator!=(MemoryArea const &RHS) const {
    return Interval<uintptr_t>::operator==(RHS);
  }
  
  /// @}
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Get a copy of this MemoryArea with a new Length.
  MemoryArea withLength(std::size_t Length) const {
    return MemoryArea(start(), Length, Access);
  }
  
  /// @}
};


} // namespace seec

#endif // SEEC_DSA_MEMORYAREA_HPP
