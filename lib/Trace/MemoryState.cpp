//===- lib/Trace/MemoryState.cpp ------------------------------------------===//
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

#include "seec/Trace/MemoryState.hpp"
#include "seec/Util/Printing.hpp"
#include "seec/Util/Range.hpp"

#include <algorithm>
#include <limits>

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// MemoryAllocation Mutators
//------------------------------------------------------------------------------

llvm::ArrayRef<char>
MemoryAllocation::getAreaData(MemoryArea const &Area) const
{
  assert(MemoryArea(Address, Size).contains(Area));

  auto const Offset = Area.address() - Address;
  return llvm::ArrayRef<char>(Data.data() + Offset, Area.length());
}

llvm::ArrayRef<unsigned char>
MemoryAllocation::getAreaInitialization(MemoryArea const &Area) const
{
  assert(MemoryArea(Address, Size).contains(Area));

  auto const Offset = Area.address() - Address;
  return llvm::ArrayRef<unsigned char>(Init.data() + Offset, Area.length());
}

void MemoryAllocation::addBlock(MappedMemoryBlock const &Block)
{
  clearArea(Block.area());

  auto const Offset = Block.address() - Address;

  std::memcpy(Data.data() + Offset, Block.data(), Block.length());

  std::memset(Init.data() + Offset,
              std::numeric_limits<unsigned char>::max(),
              Block.length());
}

void MemoryAllocation::addArea(uintptr_t const AtAddress,
                               llvm::ArrayRef<char> WithData,
                               llvm::ArrayRef<unsigned char> WithInitialization)
{
  assert(WithData.size() == WithInitialization.size());

  clearArea(MemoryArea(AtAddress, WithData.size()));

  auto const Offset = AtAddress - Address;

  std::memcpy(Data.data() + Offset,
              WithData.data(),
              WithData.size());

  std::memcpy(Init.data() + Offset,
              WithInitialization.data(),
              WithInitialization.size());
}

void MemoryAllocation::clearArea(MemoryArea const &Area)
{
  assert(MemoryArea(Address, Size).contains(Area));
  assert(Area.length() != 0);

  auto const Complete = std::numeric_limits<unsigned char>::max();

  auto const Offset = Area.address() - Address;
  auto const Length = Area.length();

  auto const InitBegin = Init.begin() + Offset;
  auto const InitEnd   = InitBegin + Length;

  // Determine this area's initialization.
  EPreviousAreaType Type = EPreviousAreaType::Partial;

  if (std::all_of(InitBegin, InitEnd,
                  [] (unsigned char const C) { return C == 0; }))
  {
    Type = EPreviousAreaType::Uninitialized;
  }
  else if (std::all_of(InitBegin, InitEnd,
           [] (unsigned char const C) { return C == Complete; }))
  {
    Type = EPreviousAreaType::Uninitialized;
  }

  PreviousType.push_back(Type);

  // This is the only case in which we need to save the initialization.
  if (Type == EPreviousAreaType::Partial)
    PreviousInit.insert(PreviousInit.end(), InitBegin, InitEnd);

  // This is the only case in which we need to save the data. This is also the
  // only case in which "clearing" the area requires us to do anything (set the
  // initialization of the bytes to zero).
  if (Type != EPreviousAreaType::Uninitialized) {
    PreviousData.insert(PreviousData.end(),
                        Data.begin() + Offset,
                        Data.begin() + Offset + Length);
    std::fill(InitBegin, InitEnd, 0);
  }
}

void MemoryAllocation::rewindArea(MemoryArea const &Area)
{
  assert(MemoryArea(Address, Size).contains(Area));
  assert(Area.length() != 0);

  auto const Complete = std::numeric_limits<unsigned char>::max();

  auto const Offset = Area.address() - Address;
  auto const Length = Area.length();

  auto const InitBegin = Init.begin() + Offset;

  assert(!PreviousType.empty());
  auto const Type = PreviousType.back();
  PreviousType.pop_back();

  // Set area as uninitialized.
  if (Type == EPreviousAreaType::Uninitialized)
    std::fill_n(InitBegin, Length, 0);

  // Restore initialization of area.
  if (Type == EPreviousAreaType::Partial) {
    assert(PreviousInit.size() >= Length);
    auto const PrevInitIt = PreviousInit.end() - Length;
    std::copy(PrevInitIt, PreviousInit.end(), InitBegin);
    PreviousInit.erase(PrevInitIt, PreviousInit.end());
  }

  // Set area as completely initialized.
  if (Type == EPreviousAreaType::Complete)
    std::fill_n(InitBegin, Length, Complete);

  // Restore data of area.
  if (Type != EPreviousAreaType::Uninitialized) {
    assert(PreviousData.size() >= Length);
    auto const PrevDataIt = PreviousData.end() - Length;
    std::copy(PrevDataIt, PreviousData.end(), Data.begin() + Offset);
    PreviousData.erase(PrevDataIt, PreviousData.end());
  }
}

void MemoryAllocation::resize(std::size_t const NewSize)
{
  Data.resize(NewSize);
  Init.resize(NewSize);
  Size = NewSize;
}

//------------------------------------------------------------------------------
// MemoryState
//------------------------------------------------------------------------------

MemoryAllocation &MemoryState::getAllocation(MemoryArea const &ForArea)
{
  assert(!Allocations.empty() && "No allocations available!");

  auto It = Allocations.upper_bound(ForArea.start());
  assert(It != Allocations.begin() && "Allocation not found!");

  --It;
  auto const AllocStart = It->second.getAddress();
  auto const AllocSize  = It->second.getSize();
  assert(MemoryArea(AllocStart, AllocSize).contains(ForArea)
          && "Allocation does not contain block!");

  return It->second;
}

MemoryAllocation const &
MemoryState::getAllocation(MemoryArea const &ForArea) const
{
  assert(!Allocations.empty() && "No allocations available!");

  auto It = Allocations.upper_bound(ForArea.start());
  assert(It != Allocations.begin() && "Allocation not found!");

  --It;
  auto const AllocStart = It->second.getAddress();
  auto const AllocSize  = It->second.getSize();
  assert(MemoryArea(AllocStart, AllocSize).contains(ForArea)
          && "Allocation does not contain block!");

  return It->second;
}

MemoryAllocation const *
MemoryState::findAllocation(uintptr_t const ForAddress) const
{
  if (Allocations.empty())
    return nullptr;

  auto It = Allocations.upper_bound(ForAddress);
  if (It == Allocations.begin())
    return nullptr;

  --It;
  auto const AllocStart = It->second.getAddress();
  auto const AllocSize  = It->second.getSize();
  if (!MemoryArea(AllocStart, AllocSize).contains(ForAddress))
    return nullptr;

  return &(It->second);
}

void MemoryState::allocationAdd(uintptr_t const Address, std::size_t const Size)
{
  if (Size == 0)
    return;

  auto const Result = Allocations.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(Address),
                                          std::forward_as_tuple(Address, Size));
  assert(Result.second && "Allocation already exists!");
}

void MemoryState::allocationRemove(uintptr_t const Address,
                                   std::size_t const Size)
{
  if (Size == 0)
    return;

  auto const It = Allocations.find(Address);
  assert(It != Allocations.end() && "Allocation does not exist!");

  PreviousAllocations.emplace(std::move(It->second));
  Allocations.erase(It);
}

void MemoryState::allocationResize(uintptr_t const Address,
                                   std::size_t const CurrentSize,
                                   std::size_t const NewSize)
{
  if (CurrentSize == 0) {
    allocationAdd(Address, NewSize);
    return;
  }
  else if (NewSize == 0) {
    allocationRemove(Address, CurrentSize);
    return;
  }

  auto &Alloc = getAllocation(MemoryArea(Address, CurrentSize));
  assert(Alloc.getSize() == CurrentSize);

  // If the allocation is shrinking, then "clear" the disappearing area so that
  // we can rewind it in the Unresize.
  if (NewSize < CurrentSize)
    Alloc.clearArea(MemoryArea(Address + NewSize,
                               CurrentSize - NewSize));

  Alloc.resize(NewSize);
}

void MemoryState::allocationUnremove(uintptr_t const Address,
                                     std::size_t const Size)
{
  if (Size == 0)
    return;

  assert(!PreviousAllocations.empty() && "No previous allocations!");

  auto &Top = PreviousAllocations.top();
  assert(Top.getAddress() == Address && "Previous allocation does not match!");

  auto const Result = Allocations.emplace(Address, std::move(Top));
  assert(Result.second && "Allocation already exists!");

  PreviousAllocations.pop();
}

void MemoryState::allocationUnadd(uintptr_t const Address,
                                  std::size_t const Size)
{
  if (Size == 0)
    return;

  auto const It = Allocations.find(Address);
  assert(It != Allocations.end() && "Allocation does not exist!");

  Allocations.erase(It);
}

void MemoryState::allocationUnresize(uintptr_t const Address,
                                     std::size_t const CurrentSize,
                                     std::size_t const NewSize)
{
  if (CurrentSize == 0) {
    allocationUnremove(Address, NewSize);
    return;
  }
  else if (NewSize == 0) {
    allocationUnadd(Address, CurrentSize);
    return;
  }

  auto &Alloc = getAllocation(MemoryArea(Address, CurrentSize));
  assert(Alloc.getSize() == CurrentSize);

  Alloc.resize(NewSize);

  // If this resize (originally) shrank the allocation, then rewind the area
  // that we have just restored.
  if (NewSize > CurrentSize)
    Alloc.rewindArea(MemoryArea(Address + CurrentSize,
                                NewSize - CurrentSize));
}

void MemoryState::addBlock(MappedMemoryBlock const &Block)
{
  getAllocation(Block.area()).addBlock(Block);
}

void MemoryState::removeBlock(MemoryArea Area)
{
  getAllocation(Area).rewindArea(Area);
}

void MemoryState::addCopy(uintptr_t const Source,
                          uintptr_t const Destination,
                          std::size_t const Size)
{
  auto const SArea = MemoryArea(Source, Size);

  auto &SAlloc = getAllocation(SArea);
  auto &DAlloc = getAllocation(MemoryArea(Destination, Size));

  DAlloc.addArea(Destination,
                 SAlloc.getAreaData(SArea),
                 SAlloc.getAreaInitialization(SArea));
}

void MemoryState::removeCopy(uintptr_t const Source,
                             uintptr_t const Destination,
                             std::size_t const Size)
{
  auto const DArea = MemoryArea(Destination, Size);
  getAllocation(DArea).rewindArea(DArea);
}

void MemoryState::addClear(MemoryArea Area)
{
  // TODO: This is temporary until we ensure that clears don't occur on
  //       unallocated regions.
  if (Allocations.empty()) {
    // llvm::errs() << "addClear(): no allocations available.\n";
    return;
  }

  auto It = Allocations.upper_bound(Area.start());
  if (It == Allocations.begin()) {
    // llvm::errs() << "addClear(): allocation not found.\n";
    return;
  }

  --It;
  auto const AllocStart = It->second.getAddress();
  auto const AllocSize  = It->second.getSize();
  if (!MemoryArea(AllocStart, AllocSize).contains(Area)) {
    // llvm::errs() << "addClear(): allocation does not contain block!\n";
    return;
  }

  It->second.clearArea(Area);
}

void MemoryState::removeClear(MemoryArea Area)
{
  // TODO: This is temporary until we ensure that clears don't occur on
  //       unallocated regions.
  if (Allocations.empty()) {
    // llvm::errs() << "addClear(): no allocations available.\n";
    return;
  }

  auto It = Allocations.upper_bound(Area.start());
  if (It == Allocations.begin()) {
    // llvm::errs() << "addClear(): allocation not found.\n";
    return;
  }

  --It;
  auto const AllocStart = It->second.getAddress();
  auto const AllocSize  = It->second.getSize();
  if (!MemoryArea(AllocStart, AllocSize).contains(Area)) {
    // llvm::errs() << "addClear(): allocation does not contain block!\n";
    return;
  }

  It->second.rewindArea(Area);
}



//------------------------------------------------------------------------------
// MemoryState::Region
//------------------------------------------------------------------------------

bool MemoryState::Region::isCompletelyInitialized() const
{
  auto const Complete = std::numeric_limits<unsigned char>::max();
  auto const Initialization = getByteInitialization();
  return std::all_of(Initialization.begin(), Initialization.end(),
                     [] (unsigned char const Byte) { return Byte == Complete;});
}

bool MemoryState::Region::isPartiallyInitialized() const
{
  auto const Initialization = getByteInitialization();
  return std::any_of(Initialization.begin(), Initialization.end(),
                     [] (unsigned char const Value) { return Value != 0; });
}

llvm::ArrayRef<unsigned char> MemoryState::Region::getByteInitialization() const
{
  return State.getAllocation(Area).getAreaInitialization(Area);
}

llvm::ArrayRef<char> MemoryState::Region::getByteValues() const
{
  return State.getAllocation(Area).getAreaData(Area);
}


//------------------------------------------------------------------------------
// MemoryState Printing
//------------------------------------------------------------------------------

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MemoryState const &State) {
  Out << " MemoryState:\n";

  for (auto const &Alloc : State.getAllocations()) {
    Out << "  @" << Alloc.first << " (" << Alloc.second.getSize() << ")\n";
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
