//===- lib/Trace/TraceMemory.cpp ------------------------------------------===//
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

#include "seec/Trace/TraceMemory.hpp"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace seec {

namespace trace {

TraceMemoryAllocation *
TraceMemoryState::getAllocationAtOrPreceding(uintptr_t const Address)
{
  // Find the first allocation starting at a higher address.
  auto It = m_Allocations.upper_bound(Address);
  if (It == m_Allocations.begin())
    return nullptr;
  
  // Rewind to the first allocation starting at an equal or lower address.
  --It;
  return &(It->second);
}

TraceMemoryAllocation const *
TraceMemoryState::getAllocationAtOrPreceding(uintptr_t const Address) const
{
  // Find the first allocation starting at a higher address.
  auto It = m_Allocations.upper_bound(Address);
  if (It == m_Allocations.begin())
    return nullptr;
  
  // Rewind to the first allocation starting at an equal or lower address.
  --It;
  return &(It->second);
}

TraceMemoryAllocation &
TraceMemoryState::getAllocationContaining(uintptr_t const Address,
                                          std::size_t const Length)
{
  auto AllocPtr = getAllocationAtOrPreceding(Address);
  assert(AllocPtr->getArea().contains(MemoryArea(Address, Length)));
  return *AllocPtr;
}

TraceMemoryAllocation const &
TraceMemoryState::getAllocationContaining(uintptr_t const Address,
                                          std::size_t const Length) const
{
  auto AllocPtr = getAllocationAtOrPreceding(Address);
  assert(AllocPtr->getArea().contains(MemoryArea(Address, Length)));
  return *AllocPtr;
}

void TraceMemoryState::add(uintptr_t Address,
                           std::size_t Length)
{
  auto &Alloc = getAllocationContaining(Address, Length);
  std::fill_n(Alloc.getShadowAt(Address), Length, getInitializedByte());
}

void TraceMemoryState::memmove(uintptr_t const Source,
                               uintptr_t const Destination,
                               std::size_t const Size)
{
  auto &SrcAlloc = getAllocationContaining(Source, Size);
  auto &DstAlloc = getAllocationContaining(Destination, Size);
  
  std::memcpy(DstAlloc.getShadowAt(Destination),
              SrcAlloc.getShadowAt(Source),
              Size);
}

void TraceMemoryState::clear(uintptr_t Address,  std::size_t Length)
{
  auto &Alloc = getAllocationContaining(Address, Length);
  std::fill_n(Alloc.getShadowAt(Address), Length, getUninitializedByte());
}

bool TraceMemoryState::hasKnownState(uintptr_t Address,
                                     std::size_t Length) const
{
  auto &Alloc = getAllocationContaining(Address, Length);
  auto const Start = Alloc.getShadowAt(Address);
  auto const End   = Start + Length;
  
  return std::all_of(Start, End,
                     [=](char const C) {
                       return C == getInitializedByte();
                     });
}

size_t TraceMemoryState::getLengthOfKnownState(uintptr_t Address,
                                               std::size_t MaxLength)
const
{
  auto &Alloc = getAllocationContaining(Address, MaxLength);
  auto const Start = Alloc.getShadowAt(Address);
  auto const End   = Start + MaxLength;
  auto const Uninit = std::find_if_not(Start, End,
                                       [=](char const C) {
                                         return C == getInitializedByte();
                                       });
  return std::distance(Start, Uninit);
}

TraceMemoryAllocation const *
TraceMemoryState::findAllocationContaining(uintptr_t const Address) const
{
  auto AllocPtr = getAllocationAtOrPreceding(Address);
  if (AllocPtr && AllocPtr->getArea().contains(Address))
    return AllocPtr;
  return nullptr;
}

void TraceMemoryState::addAllocation(uintptr_t const Address,
                                     std::size_t const Size)
{
  auto const Result =
    m_Allocations.emplace(std::make_pair(Address,
                                         TraceMemoryAllocation(Address,
                                                               Size)));

  assert(Result.second && "allocation already existed?");
}

void TraceMemoryState::removeAllocation(uintptr_t const Address)
{
  auto const It = m_Allocations.find(Address);
  assert(It != m_Allocations.end() && "allocation doesn't exist?");
  m_Allocations.erase(It);
}

void TraceMemoryState::resizeAllocation(uintptr_t const Address,
                                        std::size_t const NewSize)
{
  auto const It = m_Allocations.find(Address);
  assert(It != m_Allocations.end() && "allocation doesn't exist?");
  It->second.resize(NewSize);
}

} // namespace trace (in seec)

} // namespace seec
