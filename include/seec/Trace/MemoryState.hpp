//===- include/seec/Trace/MemoryState.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_MEMORYSTATE_HPP
#define SEEC_TRACE_MEMORYSTATE_HPP

#include "seec/DSA/MappedMemoryBlock.hpp"
#include "seec/Trace/StateCommon.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/ArrayRef.h"

#include <stack>
#include <thread>
#include <map>
#include <vector>

namespace llvm {
  class raw_ostream;
}

namespace seec {

namespace trace {


/// \brief A single recreated memory allocation.
///
class MemoryAllocation {
  /// The address that this allocation starts at.
  stateptr_ty Address;

  /// The size of this allocation (in \c char units).
  std::size_t Size;

  /// The value of each \c char in this allocation.
  std::vector<char> Data;

  /// The initialization of each \c char in this allocation (truth indicates
  /// initialization, i.e. a 1 bit is initialized and a 0 is uninitialized).
  std::vector<unsigned char> Init;

  /// Determines the initialization of a "saved" area (overwritten or cleared).
  enum class EPreviousAreaType : uint8_t {
    Uninitialized,
    Partial,
    Complete
  };

  /// Determines the initialization of all "saved" areas, in order from oldest
  /// to most recent (i.e. the most recent is at the end of the vector).
  std::vector<EPreviousAreaType> PreviousType;

  /// Holds the value of "saved" areas, in order from oldest to most recent.
  /// For example, if the most recently saved area was 4 chars, then the final
  /// 4 chars of this vector will hold the saved chars (if the type is either
  /// \c EPreviousAreaType::Partial or \c EPreviousAreaType::Complete.
  std::vector<char> PreviousData;

  /// Holds the initialization of "saved" areas, in order from oldest to most
  /// recent. For example, if the most recently saved area was 4 chars, then
  /// the final 4 chars of this vector will hold its initialization (if the
  /// type is \c EPreviousAreaType::Partial.
  std::vector<unsigned char> PreviousInit;

  MemoryAllocation(MemoryAllocation const &) = delete;
  MemoryAllocation &operator=(MemoryAllocation const &) = delete;

public:
  /// \brief Construct a new \c MemoryAllocation.
  ///
  explicit MemoryAllocation(stateptr_ty const WithAddress,
                            std::size_t const WithSize)
  : Address(WithAddress),
    Size(WithSize),
    Data(Size),
    Init(Size),
    PreviousType(),
    PreviousData(),
    PreviousInit()
  {}

  MemoryAllocation(MemoryAllocation &&Other) = default;
  MemoryAllocation &operator=(MemoryAllocation &&RHS) = default;

  /// \brief Get this allocation's start address.
  ///
  stateptr_ty const getAddress() const { return Address; }

  /// \brief Get the size of this allocation, in \c char units.
  ///
  std::size_t const getSize() const { return Size; }

  /// \brief Get the raw values of the allocated \c chars.
  ///
  llvm::ArrayRef<char>
  getAreaData(MemoryArea const &Area) const;

  /// \brief Get the initialization of the allocated \c chars.
  ///
  llvm::ArrayRef<unsigned char>
  getAreaInitialization(MemoryArea const &Area) const;

  /// \brief Find out if the contained bytes are initialized.
  ///
  bool isCompletelyInitialized() const;

  /// \brief Find out if any contained byte is initialized.
  /// If the region is completely initialized, this method will also return
  /// true.
  ///
  bool isPartiallyInitialized() const;

  /// \brief Find out if all contained bytes are uninitialized.
  ///
  bool isUninitialized() const;

  /// \brief Add a new \c MappedMemoryBlock, updating the value and
  ///        initialization of the contained memory.
  ///
  void addBlock(MappedMemoryBlock const &Block);

  /// \brief Set the raw values and initialization of the memory starting at
  ///        a the given address (contained within this allocation). The
  ///        current value and initialization of the area will be saved.
  ///
  void addArea(stateptr_ty const AtAddress,
               llvm::ArrayRef<char> WithData,
               llvm::ArrayRef<unsigned char> WithInitialization);

  /// \brief Clear the given memory area (set it to be uninitialized). The
  ///        current value and initialization of the area will be saved.
  ///
  void clearArea(MemoryArea const &Area);

  /// \brief Rewind the given memory area, restoring its value and
  ///        initialization from the most recently saved state.
  ///
  void rewindArea(MemoryArea const &Area);

  /// \brief Change the size of this allocation.
  ///        If the \c NewSize is larger than the current size, the added chars
  ///        will be uninitialized.
  ///
  void resize(std::size_t const NewSize);
};


class MemoryState;


/// \brief Represents a single region of MemoryState.
///
class MemoryStateRegion {
  MemoryState const &State; ///< The state that this region belongs to.

  MemoryArea const Area; ///< The area that this region covers.

public:
  /// \name Constructors
  /// @{

  /// \brief Construct a new Region covering an area in the given state.
  ///
  MemoryStateRegion(MemoryState const &InState, MemoryArea RegionArea)
  : State(InState),
    Area(RegionArea)
  {}

  /// @} (Constructors)

  /// \name Accessors
  /// @{

  /// \brief Get the state that this region belongs to.
  ///
  MemoryState const &getState() const { return State; }

  /// \brief Get the area that this region covers.
  ///
  MemoryArea const &getArea() const { return Area; }

  /// @}

  /// \name Queries
  /// @{

  /// \brief Find out if an allocation covers this area.
  ///
  bool isAllocated() const;

  /// \brief Find out if the contained bytes are initialized.
  ///
  bool isCompletelyInitialized() const;

  /// \brief Find out if any contained byte is initialized.
  /// If the region is completely initialized, this method will also return
  /// true.
  ///
  bool isPartiallyInitialized() const;

  /// \brief Find out if the contained bytes are uninitialized.
  ///
  bool isUninitialized() const;

  /// \brief Find out whether each contained byte is initialized.
  ///
  llvm::ArrayRef<unsigned char> getByteInitialization() const;

  /// \brief Find out the value of each contained byte.
  ///
  /// Uninitialized bytes will have a value of zero.
  ///
  llvm::ArrayRef<char> getByteValues() const;

  /// @}
};


/// \brief The complete reconstructed state of memory.
///
/// The memory state is stored as a collection of \c MemoryAllocation objects.
/// Each \c MemoryAllocation holds the state and initialization of a single
/// memory allocation (e.g. an alloca or a dynamically allocated area).
///
class MemoryState {
  /// Map allocation start addresses to the allocations themselves.
  std::map<stateptr_ty, MemoryAllocation> Allocations;

  /// Historical allocations (that were deallocated).
  std::stack<MemoryAllocation> PreviousAllocations;

  // Don't allow copying
  MemoryState(MemoryState const &) = delete;
  MemoryState &operator=(MemoryState const &) = delete;

public:
  /// \brief Construct an empty MemoryState.
  ///
  MemoryState()
  : Allocations(),
    PreviousAllocations()
  {}


  /// \name Accessors
  /// @{

  /// \brief Get the map of current allocations.
  ///
  decltype(Allocations) const &getAllocations() const { return Allocations; }

  /// \brief Get the \c MemoryAllocation that contains the given \c MemoryArea.
  ///        This method will assert if no such allocation exists.
  ///
  MemoryAllocation &getAllocation(MemoryArea const &ForArea);

  /// \brief Get the \c MemoryAllocation that contains the given \c MemoryArea.
  ///        This method will assert if no such allocation exists.
  ///
  MemoryAllocation const &getAllocation(MemoryArea const &ForArea) const;

  /// \brief Find the \c MemoryAllocation that contains the given \c MemoryArea.
  ///        This method will return \c nullptr if no such allocation exists.
  ///
  MemoryAllocation const *findAllocation(stateptr_ty const ForAddress) const;

  /// @} (Accessors)


  /// \name Mutators
  /// @{

  /// \brief Add a new allocation (moving forward).
  ///
  void allocationAdd(stateptr_ty const Address, std::size_t const Size);

  /// \brief Remove an allocation (moving forward).
  ///
  void allocationRemove(stateptr_ty const Address, std::size_t const Size);

  /// \brief Resize an allocation (moving forward).
  ///
  void allocationResize(stateptr_ty const Address,
                        std::size_t const CurrentSize,
                        std::size_t const NewSize);

  /// \brief Unremove an allocation (moving backward).
  ///
  void allocationUnremove(stateptr_ty const Address, std::size_t const Size);

  /// \brief Unadd an allocation (moving backward).
  ///
  void allocationUnadd(stateptr_ty const Address, std::size_t const Size);

  /// \brief Resize an allocation (moving backward).
  ///
  void allocationUnresize(stateptr_ty const Address,
                          std::size_t const CurrentSize,
                          std::size_t const NewSize);

  /// \brief Add the given \c MappedMemoryBlock to the memory state, setting
  ///        the raw values of the area to the values contained in the block,
  ///        and setting the area to be completely initialized.
  ///
  void addBlock(MappedMemoryBlock const &Block);

  /// \brief Remove the current state from the given \c MemoryArea, rewinding
  ///        its values and initialization to the previous state.
  ///
  void removeBlock(MemoryArea Area);

  /// \brief Copy the value and initialization of memory beginning at \c Source
  ///        to the memory beginning at \c Destination. \c Size \c char units
  ///        of state and initialization will be copied.
  ///
  void addCopy(stateptr_ty const Source,
               stateptr_ty const Destination,
               std::size_t const Size);

  /// \brief Rewind a previous copy from \c Source to \c Destination. The value
  ///        and initialization of memory at \c Destination will be rewound to
  ///        its state prior to the copy.
  ///
  void removeCopy(stateptr_ty const Source,
                  stateptr_ty const Destination,
                  std::size_t const Size);

  /// \brief Clear the given \c MemoryArea, setting it to be uninitialized.
  ///
  void addClear(MemoryArea Area);

  /// \brief Rewind a clear to the given \c MemoryArea, restoring its
  ///        initialization to its state prior to the clear.
  ///
  void removeClear(MemoryArea Area);

  /// @} (Mutators)


  /// \name Regions
  /// @{

  /// \brief Get a Region covering the given area.
  ///
  MemoryStateRegion getRegion(MemoryArea Area) const {
    return MemoryStateRegion(*this, Area);
  }

  /// @} (Regions)
};

/// \brief Print a textual description of a MemoryState.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MemoryState const &State);


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_MEMORYSTATE_HPP
