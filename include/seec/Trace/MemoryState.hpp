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
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Maybe.hpp"

#include <thread>
#include <map>
#include <vector>

namespace seec {

namespace trace {


/// \brief A fragment of a reconstructed memory state.
///
class MemoryStateFragment {
  /// The block of memory for this fragment.
  MappedMemoryBlock Block;

  /// The event that created this block of memory.
  EventLocation StateRecord;

public:
  /// \name Constructors
  /// @{

  /// \brief Constructor.
  MemoryStateFragment(MappedMemoryBlock TheBlock, EventLocation TheEvent)
  : Block(std::move(TheBlock)),
    StateRecord(std::move(TheEvent))
  {}

  /// \brief Copy constructor.
  MemoryStateFragment(MemoryStateFragment const &Other) = default;

  /// \brief Move constructor.
  MemoryStateFragment(MemoryStateFragment &&Other)
  : Block(std::move(Other.Block)),
    StateRecord(std::move(Other.StateRecord))
  {}

  /// @} (Constructors)


  /// \name Assignment
  /// @{

  /// \brief Copy assignment.
  MemoryStateFragment &operator=(MemoryStateFragment const &RHS) = default;

  /// \brief Move assignment.
  MemoryStateFragment &operator=(MemoryStateFragment &&RHS) {
    Block = std::move(RHS.Block);
    StateRecord = std::move(RHS.StateRecord);
    return *this;
  }

  /// @} (Assignment)


  /// \name Accessors
  /// @{

  /// \brief Get a reference to the block of memory for this fragment.
  MappedMemoryBlock &getBlock() { return Block; }

  /// \brief Get a const reference to the block of memory for this fragment.
  MappedMemoryBlock const &getBlock() const { return Block; }

  /// \brief Get the EventLocation for the event that caused this state.
  EventLocation const &getStateRecordLocation() const { return StateRecord; }

  /// @} (Accessors)
};


/// \brief The complete reconstructed state of memory.
///
/// The memory state is stored as a collection of fragments. Each fragment
/// represents the state caused by a single event, such as a store or memcpy().
/// The fragments are kept in an ordered map, that maps the start address of
/// the fragment to a MemoryStateFragment object.
///
class MemoryState {
  /// Map fragments' start addresses to the fragments themselves.
  std::map<uintptr_t, MemoryStateFragment> FragmentMap;

  // Don't allow copying
  MemoryState(MemoryState const &) = delete;
  MemoryState &operator=(MemoryState const &) = delete;

public:
  /// \brief Construct an empty MemoryState.
  MemoryState()
  : FragmentMap()
  {}


  /// \name Accessors
  /// @{

  /// \brief Get the map from start addresses to fragments.
  ///
  decltype(FragmentMap) const &getFragmentMap() const { return FragmentMap; }

  /// \brief Get the total number of fragments.
  ///
  std::size_t getNumberOfFragments() const { return FragmentMap.size(); }

  /// @} (Accessors)


  /// \name Mutators
  /// @{

  /// \brief Add a memory block caused by an event.
  ///
  void add(MappedMemoryBlock const &Block, EventLocation Event);
  
  /// \brief Add a memory block caused by an event.
  ///
  void add(MappedMemoryBlock &&Block, EventLocation Event);
  
  /// \brief Copy a region of memory to a new region.
  ///
  void memcpy(uintptr_t Source,
              uintptr_t Destination,
              std::size_t Size,
              EventLocation Event);

  /// \brief Clear a region of memory.
  ///
  void clear(MemoryArea Area);
  
  /// \brief Merge two (previously split) fragments.
  ///
  void unsplit(uintptr_t LeftAddress, uintptr_t RightAddress);
  
  /// \brief Extend the block at the given Address by TrimSize.
  ///
  void untrimRightSide(uintptr_t Address, std::size_t TrimSize);
  
  /// \brief Move the block at Address to its PriorAddress.
  ///
  void untrimLeftSide(uintptr_t Address, uintptr_t PriorAddress);

  /// @} (Mutators)


  /// \name Regions
  /// @{

  /// \brief Represents a single region of MemoryState.
  ///
  class Region {
    MemoryState const &State; ///< The state that this region belongs to.

    MemoryArea const Area; ///< The area that this region covers.

  public:
    /// \name Constructors
    /// @{

    /// \brief Construct a new Region covering an area in the given state.
    ///
    Region(MemoryState const &InState, MemoryArea RegionArea)
    : State(InState),
      Area(RegionArea)
    {}

    /// \brief Copy constructor.
    Region(Region const &) = default;

    /// \brief Move constructor.
    Region(Region &&) = default;

    /// @} (Constructors)

    /// \name Accessors
    /// @{

    /// \brief Get the state that this region belongs to.
    MemoryState const &getState() const { return State; }

    /// \brief Get the area that this region covers.
    MemoryArea const &getArea() const { return Area; }

    /// @}

    /// \name Queries
    /// @{

    /// \brief Find out if the contained bytes are initialized.
    bool isCompletelyInitialized() const;

    /// \brief Find out whether each contained byte is initialized.
    std::vector<char> getByteInitialization() const;
    
    /// \brief Find out the value of each contained byte.
    ///
    /// Uninitialized bytes will have a value of zero.
    ///
    std::vector<char> getByteValues() const;

    /// @}
  };

  /// \brief Get a Region covering the given area.
  ///
  Region getRegion(MemoryArea Area) const {
    return Region(*this, Area);
  }

  /// @} (Regions)
};

/// Print a textual description of a MemoryState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MemoryState const &State);


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_MEMORYSTATE_HPP
