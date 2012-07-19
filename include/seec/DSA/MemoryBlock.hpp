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
#include <new>

namespace seec {


/// Stores a contiguous block of memory.
class MemoryBlock : public MemoryArea {
private:
  /// The data in this MemoryBlock.
  char *Data;

public:
  /// Default constructor.
  MemoryBlock()
  : MemoryArea(),
    Data(nullptr)
  {}

  /// Initializing constructor.
  MemoryBlock(uint64_t Start, uint64_t Length, char const *Data)
  : MemoryArea(Start, Length),
    Data(nullptr)
  {
    if (Length) {
      this->Data = new (std::nothrow) char[Length];

      if (this->Data) {
        memcpy(this->Data, Data, Length);
      }
      else {
        setStartEnd(0, 0);
      }
    }
  }

  /// Initializing constructor.
  MemoryBlock(void const *Start, size_t Length)
  : MemoryArea(reinterpret_cast<uintptr_t>(Start), Length),
    Data(nullptr)
  {
    if (Length) {
      Data = new (std::nothrow) char[Length];

      if (this->Data) {
        memcpy(Data, Start, Length);
      }
      else {
        setStartEnd(0, 0);
      }
    }
  }

  /// Copy constructor.
  MemoryBlock(MemoryBlock const &Other)
  : MemoryArea(Other),
    Data(nullptr)
  {
    if (length()) {
      Data = new (std::nothrow) char[length()];

      if (Data) {
        memcpy(Data, Other.Data, length());
      }
      else {
        setStartEnd(0, 0);
      }
    }
  }

  /// Move constructor.
  MemoryBlock(MemoryBlock &&Other)
  : MemoryArea(Other),
    Data(Other.Data)
  {
    Other.setStartEnd(0, 0);
    Other.Data = nullptr;
  }

  /// Destructor.
  ~MemoryBlock()
  {
    if (Data)
      delete[] Data;
  }

  /// Copy assignment.
  MemoryBlock & operator=(MemoryBlock const &RHS) {
    if (Data)
      delete[] Data;

    MemoryArea::operator=(RHS);

    if (length()) {
      Data = new (std::nothrow) char[length()];

      if (Data) {
        memcpy(Data, RHS.Data, length());
      }
      else {
        setStartEnd(0, 0);
      }
    }
    else
      Data = nullptr;

    return *this;
  }

  /// Move assignment.
  MemoryBlock & operator=(MemoryBlock &&RHS) {
    if (Data)
      delete[] Data;

    MemoryArea::operator=(RHS);
    Data = RHS.Data;

    RHS.setStartEnd(0, 0);
    RHS.Data = nullptr;

    return *this;
  }

  /// Get a pointer to the data of this MemoryBlock.
  char const *data() const { return Data; }
  
  /// Get the MemoryArea that this MemoryBlock occupies.
  MemoryArea const &area() const { return *this; }
};


} // namespace seec

#endif // SEEC_DSA_MEMORYBLOCK_HPP
