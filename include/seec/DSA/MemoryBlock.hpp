//===- include/seec/DSA/MemoryBlock.hpp ---------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_DSA_MEMORYBLOCK_HPP
#define SEEC_DSA_MEMORYBLOCK_HPP

#include "seec/DSA/MemoryArea.hpp"

#include <cstring>
#include <cstdint>
#include <memory>
#include <new>

namespace seec {


/// Stores a contiguous block of memory.
class MemoryBlock : public MemoryArea {
private:
  /// The data in this MemoryBlock.
  std::unique_ptr<char []> Data;

public:
  /// \brief Construct an empty MemoryBlock.
  ///
  MemoryBlock()
  : MemoryArea(),
    Data()
  {}

  /// \brief Construct MemoryBlock by copying the given data.
  ///
  MemoryBlock(uintptr_t Start, std::size_t Length, char const *CopyData);

  /// \brief Construct a MemoryBlock by copying data from the given location.
  ///
  MemoryBlock(void const *Start, std::size_t Length);

  /// \brief Copy the given MemoryBlock.
  ///
  MemoryBlock(MemoryBlock const &Other);

  /// \brief Move state from the given MemoryBlock.
  ///
  MemoryBlock(MemoryBlock &&Other)
  : MemoryArea(std::move(Other)),
    Data(std::move(Other.Data))
  {
    Other.setStartEnd(0, 0);
  }

  /// \brief Copy the given MemoryBlock.
  ///
  MemoryBlock &operator=(MemoryBlock const &RHS);

  /// \brief Move state from the given MemoryBlock.
  ///
  MemoryBlock &operator=(MemoryBlock &&RHS);

  /// Get a pointer to the data of this MemoryBlock.
  char const *data() const { return Data.get(); }
  
  /// Get the MemoryArea that this MemoryBlock occupies.
  MemoryArea const &area() const { return *this; }
};


} // namespace seec

#endif // SEEC_DSA_MEMORYBLOCK_HPP
