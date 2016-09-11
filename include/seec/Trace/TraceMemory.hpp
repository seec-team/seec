//===- include/seec/Trace/TraceMemory.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACEMEMORY_HPP
#define SEEC_TRACE_TRACEMEMORY_HPP

#include "seec/DSA/MemoryArea.hpp"

#include <cassert>
#include <map>
#include <memory>
#include <vector>

namespace seec {

namespace trace {


static constexpr char getUninitializedByte() { return ~0; }
static constexpr char getInitializedByte() { return 0; }

/// \brief Holds the shadow state for a memory allocation.
///
class TraceMemoryAllocation {
  uintptr_t m_Address;
  
  std::vector<char> m_Shadow;
  
public:
  /// \brief Construct a new MemoryAllocation.
  TraceMemoryAllocation(uintptr_t const Address,
                        std::size_t const Length)
  : m_Address(Address),
    m_Shadow(/* count */ Length, /* value */ getUninitializedByte())
  {}
  
  MemoryArea getArea() const { return MemoryArea(m_Address, getLength()); }
  
  uintptr_t getAddress() const { return m_Address; }
  
  std::size_t getLength() const { return m_Shadow.size(); }
  
  char *getShadow() { return m_Shadow.data(); }
  
  char *getShadowAt(uintptr_t const Address) {
    assert(Address >= m_Address);
    assert((Address - m_Address) < m_Shadow.size());
    return m_Shadow.data() + (Address - m_Address);
  }
  
  char const *getShadowAt(uintptr_t const Address) const {
    assert(Address >= m_Address);
    assert((Address - m_Address) < m_Shadow.size());
    return m_Shadow.data() + (Address - m_Address);
  }
  
  void resize(std::size_t const NewLength) {
    m_Shadow.resize(NewLength, getUninitializedByte());
  }
};


/// \brief Holds information about traced memory states.
///
class TraceMemoryState {
  // don't allow copying
  TraceMemoryState(TraceMemoryState const &) = delete;
  TraceMemoryState &operator=(TraceMemoryState const &) = delete;
  
  /// Map from start addresses to allocations.
  std::map<uintptr_t, TraceMemoryAllocation> m_Allocations;

  TraceMemoryAllocation *
  getAllocationAtOrPreceding(uintptr_t const Address);
  
  TraceMemoryAllocation const *
  getAllocationAtOrPreceding(uintptr_t const Address) const;
  
  TraceMemoryAllocation &getAllocationContaining(uintptr_t const Address,
                                                 std::size_t const Length);

  TraceMemoryAllocation const &
  getAllocationContaining(uintptr_t const Address,
                          std::size_t const Length) const;

public:
  /// Construct a new, empty TraceMemoryState.
  TraceMemoryState()
  : m_Allocations()
  {}

  /// \brief Set all bytes in the given range to completely initialized.
  ///
  /// \param Address start address of the updated memory.
  /// \param Length number of bytes of updated memory.
  void add(uintptr_t Address, std::size_t Length);
  
  /// \brief Copy initialization of bytes in the given range.
  ///
  /// \param Source start address of the source of the memmove.
  /// \param Destination start address of the destination of the memmove.
  /// \param Size number of bytes to move.
  void memmove(uintptr_t const Source,
               uintptr_t const Destination,
               std::size_t const Size);
  
  /// \brief Set all bytes in the given range to be uninitialized.
  ///
  void clear(uintptr_t Address, std::size_t Length);
  
  /// \brief Check if all bytes in the range are completely initialized.
  ///
  bool hasKnownState(uintptr_t Address, std::size_t Length) const;
  
  /// \brief Find the length of initialized memory beginning at \c Address and
  ///        to a maximum length of \c MaxLength
  ///
  size_t getLengthOfKnownState(uintptr_t Address, std::size_t MaxLength) const;
  
  TraceMemoryAllocation const *
  findAllocationContaining(uintptr_t const Address) const;
  
  void addAllocation(uintptr_t const Address, std::size_t const Size);
  
  void removeAllocation(uintptr_t const Address);
  
  void resizeAllocation(uintptr_t const Address, std::size_t const NewSize);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEMEMORY_HPP
