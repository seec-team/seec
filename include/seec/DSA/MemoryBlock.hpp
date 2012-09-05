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
  MemoryBlock(uint64_t Start, uint64_t Length, char const *CopyData)
  : MemoryArea(Start, Length),
    Data()
  {
    if (Length) {
      Data.reset(new (std::nothrow) char[Length]);
      
      if (Data)
        memcpy(Data.get(), CopyData, Length);
      else
        setStartEnd(0, 0);
    }
  }

  /// \brief Construct a MemoryBlock by copying data from the given location.
  ///
  MemoryBlock(void const *Start, size_t Length)
  : MemoryArea(reinterpret_cast<uintptr_t>(Start), Length),
    Data()
  {
    if (Length) {
      Data.reset(new (std::nothrow) char[Length]);

      if (Data)
        memcpy(Data.get(), Start, Length);
      else
        setStartEnd(0, 0);
    }
  }

  /// \brief Copy the given MemoryBlock.
  ///
  MemoryBlock(MemoryBlock const &Other)
  : MemoryArea(Other),
    Data()
  {
    if (length()) {
      Data.reset(new (std::nothrow) char[length()]);

      if (Data)
        memcpy(Data.get(), Other.Data.get(), length());
      else
        setStartEnd(0, 0);
    }
  }

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
  MemoryBlock & operator=(MemoryBlock const &RHS) {
    MemoryArea::operator=(RHS);

    if (length()) {
      Data.reset(new (std::nothrow) char[length()]);

      if (Data)
        memcpy(Data.get(), RHS.Data.get(), length());
      else
        setStartEnd(0, 0);
    }
    else
      Data.reset();

    return *this;
  }

  /// \brief Move state from the given MemoryBlock.
  ///
  MemoryBlock & operator=(MemoryBlock &&RHS) {
    MemoryArea::operator=(RHS);
    Data = std::move(RHS.Data);

    RHS.setStartEnd(0, 0);

    return *this;
  }

  /// Get a pointer to the data of this MemoryBlock.
  char const *data() const { return Data.get(); }
  
  /// Get the MemoryArea that this MemoryBlock occupies.
  MemoryArea const &area() const { return *this; }
};


} // namespace seec

#endif // SEEC_DSA_MEMORYBLOCK_HPP
