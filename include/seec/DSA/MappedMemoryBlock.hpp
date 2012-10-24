//===- include/seec/DSA/MappedMemoryBlock.hpp ----------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_DSA_MAPPEDMEMORYBLOCK_HPP
#define SEEC_DSA_MAPPEDMEMORYBLOCK_HPP

#include "seec/DSA/MemoryArea.hpp"

#include <cassert>
#include <cstring>
#include <cstdint>

namespace seec {


/// Stores a view of a contiguous block of memory.
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
  MappedMemoryBlock(uintptr_t Start, std::size_t Length, char const *Data)
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
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Move the left hand side to a new, higher, start address.
  ///
  void trimLeftSide(uintptr_t NewStartAddress) {
    assert(NewStartAddress >= start());
    
    auto const MoveSize = NewStartAddress - start();
    setStart(NewStartAddress);
    Data += MoveSize;
  }
  
  /// \brief Move the left hand side to a new, lower, start address.
  ///
  void untrimLeftSide(uintptr_t NewStartAddress) {
    assert(NewStartAddress <= start());
    
    auto const MoveSize = NewStartAddress - start();
    setStart(NewStartAddress);
    Data += MoveSize;
  }
  
  /// @}
};


} // namespace seec

#endif // SEEC_DSA_MAPPEDMEMORYBLOCK_HPP
