//===- include/seec/Trace/TraceMemory.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
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
  TraceMemoryFragment(uint64_t Address,
                      uint64_t Length,
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


///
class OverwrittenMemoryInfo {
  std::vector<TraceMemoryFragment> States;
  
public:
  /// Constructor.
  OverwrittenMemoryInfo()
  : States()
  {}
  
  /// Copy constructor.
  OverwrittenMemoryInfo(OverwrittenMemoryInfo const &Other) = default;
  
  /// Move constructor.
  OverwrittenMemoryInfo(OverwrittenMemoryInfo &&Other)
  : States(std::move(Other.States))
  {}
  
  /// Copy assignment.
  OverwrittenMemoryInfo &operator=(OverwrittenMemoryInfo const &RHS) = default;
  
  /// Move assignment.
  OverwrittenMemoryInfo &operator=(OverwrittenMemoryInfo &&RHS) {
    States = std::move(RHS.States);
    return *this;
  }
  
  /// \name Accessors
  /// @{
  
  std::vector<TraceMemoryFragment> const &states() const { return States; }
  
  /// @}
  
  /// \name Mutators
  /// @{
  
  void add(TraceMemoryFragment Fragment) {
    States.push_back(Fragment);
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
  std::map<uint64_t, TraceMemoryFragment> Fragments;

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
  /// \param StateRecordOffset offset of the new memory state.
  /// \return an ordered set of offsets of overwritten memory states.
  OverwrittenMemoryInfo add(uint64_t Address,
                            uint64_t Length,
                            uint32_t ThreadID,
                            offset_uint StateRecordOffset,
                            uint64_t ProcessTime);
  
  /// \brief Clear a section of memory and return the removed states.
  ///
  OverwrittenMemoryInfo clear(uint64_t Address,
                              uint64_t Length);
  
  /// \brief 
  bool hasKnownState(uint64_t Address, uint64_t Length) const;
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEMEMORY_HPP
