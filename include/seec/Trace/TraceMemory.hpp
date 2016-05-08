//===- include/seec/Trace/TraceMemory.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACEMEMORY_HPP
#define SEEC_TRACE_TRACEMEMORY_HPP

#include "seec/DSA/MemoryArea.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Util/Maybe.hpp"

#include <map>
#include <vector>

namespace seec {

namespace trace {


/// \brief Trace information for an area of memory with state set by a single
///        event.
///
class TraceMemoryFragment {
  /// The address and length of this fragment.
  MemoryArea Area;

  /// Holds the ThreadID and Offset of the relevant MemoryState record.
  EventLocation StateRecord;

  /// Process time for the MemoryState record that this fragment represents.
  uint64_t ProcessTime;

public:
  /// Construct a new fragment for a state.
  TraceMemoryFragment(uintptr_t Address,
                      std::size_t Length,
                      uint32_t ThreadID,
                      offset_uint StateRecordOffset,
                      uint64_t ProcessTime)
  : Area(Address, Length),
    StateRecord(ThreadID, StateRecordOffset),
    ProcessTime(ProcessTime)
  {}

  TraceMemoryFragment(TraceMemoryFragment const &) = default;

  TraceMemoryFragment(TraceMemoryFragment &&Other)
  : Area(std::move(Other.Area)),
    StateRecord(std::move(Other.StateRecord)),
    ProcessTime(std::move(Other.ProcessTime))
  {}

  TraceMemoryFragment &operator=(TraceMemoryFragment const &) = default;

  TraceMemoryFragment &operator=(TraceMemoryFragment &&RHS) {
    this->Area = std::move(RHS.Area);
    this->StateRecord = std::move(RHS.StateRecord);
    this->ProcessTime = std::move(RHS.ProcessTime);
    return *this;
  }


  /// \name Accessors
  /// @{
  
  MemoryArea &area() { return Area; }

  MemoryArea const &area() const { return Area; }
  
  EventLocation const &stateRecord() const { return StateRecord; }

  uint32_t threadID() const { return StateRecord.getThreadID(); }

  offset_uint stateRecordOffset() const { return StateRecord.getOffset(); }

  uint64_t processTime() const { return ProcessTime; }

  /// @} (Accessors)
  
  
  /// \name Mutators
  /// @{
  
  void reposition(MemoryArea NewArea) {
    Area = NewArea;
  }
  
  /// @}
};


/// \brief Holds information about traced memory states.
///
class TraceMemoryState {
  // don't allow copying
  TraceMemoryState(TraceMemoryState const &) = delete;
  TraceMemoryState &operator=(TraceMemoryState const &) = delete;
  
  /// Map from start addresses to memory fragments.
  std::map<uintptr_t, TraceMemoryFragment> Fragments;

public:
  /// Construct a new, empty TraceMemoryState.
  TraceMemoryState()
  : Fragments()
  {}

  /// \brief Add a new state and return overwritten states.
  ///
  /// Add a new state record to the memory state, and return an ordered set
  /// containing the offsets of all previous state records that were
  /// overwritten (either partially or wholly). The offsets will be in
  /// ascending order, which is equivalent to the order that the state events
  /// occured in.
  ///
  /// \param Address start address of the updated memory.
  /// \param Length number of bytes of updated memory.
  /// \param ThreadID the thread in which this memory state occurred.
  /// \param StateRecordOffset offset of the new memory state.
  /// \param ProcessTime the process time associated with this state change.
  /// \return information about all overwritten memory states.
  void add(uintptr_t Address,
           std::size_t Length,
           uint32_t ThreadID,
           offset_uint StateRecordOffset,
           uint64_t ProcessTime);
  
  /// \brief Add a new memmove state and return overwritten states.
  ///
  /// \param Source start address of the source of the memmove.
  /// \param Destination start address of the destination of the memmove.
  /// \param Size number of bytes to move.
  /// \param Event location of the state event responsible for this memmove.
  /// \param ProcessTime the process time associated with this state change.
  /// \return information about all overwritten memory states.
  void memmove(uintptr_t const Source,
               uintptr_t const Destination,
               std::size_t const Size,
               EventLocation const &Event,
               uint64_t const ProcessTime);
  
  /// \brief Clear a section of memory and return the removed states.
  ///
  void clear(uintptr_t Address, std::size_t Length);
  
  /// \brief 
  bool hasKnownState(uintptr_t Address, std::size_t Length) const;
  
  /// \brief Find the length of initialized memory beginning at \c Address and
  ///        to a maximum length of \c MaxLength
  ///
  size_t getLengthOfKnownState(uintptr_t Address, std::size_t MaxLength) const;
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEMEMORY_HPP
