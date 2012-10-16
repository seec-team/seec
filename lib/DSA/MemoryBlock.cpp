//===- DSA/MemoryBlock.cpp ------------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/DSA/MemoryBlock.hpp"

namespace seec {

MemoryBlock::MemoryBlock(uint64_t Start, uint64_t Length, char const *CopyData)
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

MemoryBlock::MemoryBlock(void const *Start, size_t Length)
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

MemoryBlock::MemoryBlock(MemoryBlock const &Other)
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

MemoryBlock &MemoryBlock::operator=(MemoryBlock const &RHS) {
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

MemoryBlock &MemoryBlock::operator=(MemoryBlock &&RHS) {
  MemoryArea::operator=(RHS);
  Data = std::move(RHS.Data);

  RHS.setStartEnd(0, 0);

  return *this;
}

}
