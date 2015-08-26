//===- include/seec/DSA/MappedMemoryBlock.hpp ----------------------- C++ -===//
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

#ifndef SEEC_DSA_MAPPEDMEMORYBLOCK_HPP
#define SEEC_DSA_MAPPEDMEMORYBLOCK_HPP

#include "seec/DSA/MemoryArea.hpp"

#include <cassert>
#include <cstring>
#include <cstdint>

namespace seec {


/// \brief Stores a view of a contiguous block of memory.
///
class MappedMemoryBlock : public MemoryArea {
private:
  /// The data referenced by this MappedMemoryBlock.
  char const *Data;

public:
  /// Default constructor.
  MappedMemoryBlock()
  : MemoryArea(),
    Data(nullptr)
  {}

  /// Initializing constructor.
  MappedMemoryBlock(uint64_t Start, std::size_t Length, char const *Data)
  : MemoryArea(Start, Length),
    Data(Data)
  {}

  /// Copy constructor.
  MappedMemoryBlock(MappedMemoryBlock const &Other)
  : MemoryArea(Other),
    Data(Other.Data)
  {}

  /// Move constructor.
  MappedMemoryBlock(MappedMemoryBlock &&Other)
  : MemoryArea(Other),
    Data(Other.Data)
  {}

  /// Destructor.
  ~MappedMemoryBlock() {}

  /// Copy assignment.
  MappedMemoryBlock & operator=(MappedMemoryBlock const &RHS) = default;

  /// Move assignment.
  MappedMemoryBlock & operator=(MappedMemoryBlock &&RHS) {
    MemoryArea::operator=(RHS);
    Data = RHS.Data;
    return *this;
  }
  
  
  /// \name Accessors
  /// @{

  /// Get a pointer to the data of this MappedMemoryBlock.
  char const *data() const { return Data; }
  
  /// Get the MemoryArea that this MappedMemoryBlock occupies.
  MemoryArea const &area() const { return *this; }
  
  /// @} (Accessors)
};


} // namespace seec

#endif // SEEC_DSA_MAPPEDMEMORYBLOCK_HPP
