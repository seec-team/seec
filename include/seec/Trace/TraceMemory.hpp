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


///
class StateOverwrite {
public:
  /// Define the type of overwrite that occured.
  enum class OverwriteType {
    ReplaceState,      ///< Completely overwrite an existing whole state.
    ReplaceFragment,   ///< Completely overwrite an existing fragment.
    TrimFragmentRight, ///< Overwrite the right side of an existing fragment.
    TrimFragmentLeft,  ///< Overwrite the left side of an existing fragment.
    SplitFragment      ///< Overwrite the middle of an existing fragment.
  };
  
private:
  /// The type of this overwrite.
  OverwriteType Type;
  
  EventLocation State;
  
  MemoryArea OldArea;
  
  MemoryArea NewArea;
  
  /// \brief Constructor.
  StateOverwrite(OverwriteType TheType,
                 EventLocation TheState,
                 MemoryArea TheOldArea,
                 MemoryArea TheNewArea)
  : Type(TheType),
    State(TheState),
    OldArea(TheOldArea),
    NewArea(TheNewArea)
  {}
  
public:
  /// \brief Copy constructor.
  StateOverwrite(StateOverwrite const &) = default;
  
  /// \brief Move constructor.
  StateOverwrite(StateOverwrite &&) = default;
  
  /// \brief Copy assignment.
  StateOverwrite &operator=(StateOverwrite const &) = default;
  
  /// \brief Move assignment.
  StateOverwrite &operator=(StateOverwrite &&) = default;

  /// \brief Create a StateOverwrite describing the complete replacement of a
  ///        Fragment.
  /// \param Fragment the overwritten fragment.
  static StateOverwrite forReplace(TraceMemoryFragment const &Fragment) {
    // TODO: We should check if the Fragment is in fact a complete state, in
    // which case the type should be ReplaceState.
    return StateOverwrite(OverwriteType::ReplaceFragment,
                          Fragment.stateRecord(),
                          Fragment.area(),
                          Fragment.area());
  }
  
  /// \brief Create a StateOverwrite describing the overwriting of the right
  ///        (end) of a Fragment.
  /// \param Fragment the overwritten fragment.
  /// \param TrimmedEnd the trimmed end address of the fragment.
  static
  StateOverwrite forTrimFragmentRight(TraceMemoryFragment const &Fragment,
                                      uintptr_t TrimmedEnd) {
    return StateOverwrite(OverwriteType::TrimFragmentRight,
                          Fragment.stateRecord(),
                          Fragment.area(),
                          MemoryArea(TrimmedEnd,
                                     Fragment.area().end() - TrimmedEnd));
  }
  
  /// \brief Create a StateOverwrite describing the overwriting of the left
  ///        (beginning) of a Fragment.
  /// \param Fragment the overwritten fragment.
  /// \param TrimmedStart the trimmed start address of the fragment.
  static StateOverwrite forTrimFragmentLeft(TraceMemoryFragment const &Fragment,
                                            uintptr_t TrimmedStart) {
    return StateOverwrite(OverwriteType::TrimFragmentLeft,
                          Fragment.stateRecord(),
                          Fragment.area(),
                          MemoryArea(Fragment.area().start(),
                                     TrimmedStart - Fragment.area().start()));
  }
  
  /// \brief Create a StateOverwrite describing the overwrite of an internal
  ///        section of a Fragment.
  /// 
  static StateOverwrite forSplitFragment(TraceMemoryFragment const &Fragment,
                                         MemoryArea const &Overwritten) {
    return StateOverwrite(OverwriteType::SplitFragment,
                          Fragment.stateRecord(),
                          Fragment.area(),
                          Overwritten);
  }
  
  
  /// \name Accessors
  /// @{
  
  OverwriteType getType() const { return Type; }
  
  EventLocation const &getStateEvent() const { return State; }
  
  MemoryArea const &getOldArea() const { return OldArea; }
  
  MemoryArea const &getNewArea() const { return NewArea; }
  
  /// @}
};


/// \brief Contains information about all overwritten states caused by a new
///        memory state.
class OverwrittenMemoryInfo {
  std::vector<StateOverwrite> Overwrites;
  
public:
  /// \name Construction and assignment.
  /// @{
  
  /// Constructor.
  OverwrittenMemoryInfo()
  : Overwrites()
  {}
  
  /// Copy constructor.
  OverwrittenMemoryInfo(OverwrittenMemoryInfo const &Other) = default;
  
  /// Move constructor.
  OverwrittenMemoryInfo(OverwrittenMemoryInfo &&Other) = default;
  
  /// Copy assignment.
  OverwrittenMemoryInfo &operator=(OverwrittenMemoryInfo const &RHS) = default;
  
  /// Move assignment.
  OverwrittenMemoryInfo &operator=(OverwrittenMemoryInfo &&RHS) = default;
  
  /// @}
  
  
  /// \name Accessors
  /// @{
  
  /// \brief Access our collection of StateOverwrite objects.
  std::vector<StateOverwrite> const &overwrites() const { return Overwrites; }
  
  /// @}
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Add a new StateOverwrite.
  void add(StateOverwrite const &Fragment) {
    Overwrites.push_back(Fragment);
  }
  
  /// \brief Add a new StateOverwrite.
  void add(StateOverwrite &&Fragment) {
    Overwrites.push_back(Fragment);
  }
  
  /// @}
};


/// \brief Holds information about a copied memory fragment.
///
class StateCopy {
  EventLocation Event; ///< Location of the original state event.
  
  MemoryArea Area; ///< The area that was copied from.

public:
  /// \brief Create a new StateCopy.
  StateCopy(EventLocation CopiedEvent, MemoryArea CopiedArea)
  : Event(CopiedEvent),
    Area(CopiedArea)
  {}
  
  /// \brief Get the location of the original state event.
  EventLocation const &getEvent() const { return Event; }
  
  /// \brief Get the area that was copied from.
  MemoryArea const &getArea() const { return Area; }
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
  OverwrittenMemoryInfo add(uintptr_t Address,
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
  std::pair<OverwrittenMemoryInfo, std::vector<StateCopy>>
  memmove(uintptr_t const Source,
          uintptr_t const Destination,
          std::size_t const Size,
          EventLocation const &Event,
          uint64_t const ProcessTime);
  
  /// \brief Clear a section of memory and return the removed states.
  ///
  OverwrittenMemoryInfo clear(uintptr_t Address,
                              std::size_t Length);
  
  /// \brief 
  bool hasKnownState(uintptr_t Address, std::size_t Length) const;
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEMEMORY_HPP
