//===- include/seec/Trace/MemoryState.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_MEMORYSTATE_HPP
#define SEEC_TRACE_MEMORYSTATE_HPP

#include "seec/DSA/MappedMemoryBlock.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Maybe.hpp"

#include <thread>
#include <map>

namespace seec {

namespace trace {


/// \brief 
class MemoryStateFragment {
  /// The block of memory for this fragment.
  MappedMemoryBlock Block;
  
  /// The event that created this block of memory.
  EventLocation StateRecord;
  
public:
  /// Constructor.
  MemoryStateFragment(MappedMemoryBlock TheBlock, EventLocation TheEvent)
  : Block(std::move(TheBlock)),
    StateRecord(std::move(TheEvent))
  {}

  /// Copy constructor.
  MemoryStateFragment(MemoryStateFragment const &Other) = default;

  /// Move constructor.
  MemoryStateFragment(MemoryStateFragment &&Other)
  : Block(std::move(Other.Block)),
    StateRecord(std::move(Other.StateRecord))
  {}
  
  /// Copy assignment.
  MemoryStateFragment &operator=(MemoryStateFragment const &RHS) = default;
  
  /// Move assignment.
  MemoryStateFragment &operator=(MemoryStateFragment &&RHS) {
    Block = std::move(RHS.Block);
    StateRecord = std::move(RHS.StateRecord);
    return *this;
  }
  
  /// \name Accessors
  /// @{
  
  MappedMemoryBlock &getBlock() { return Block; }
  
  MappedMemoryBlock const &getBlock() const { return Block; }
  
  EventLocation const &getStateRecordLocation() const { return StateRecord; }
  
  /// @}
};


/// \brief Represents the complete state of memory.
class MemoryState {
  std::map<uint64_t, MemoryStateFragment> FragmentMap;
  
  // Don't allow copying
  MemoryState(MemoryState const &) = delete;
  MemoryState &operator=(MemoryState const &) = delete;
  
public:
  /// Constructor.
  MemoryState()
  : FragmentMap()
  {}
  
  
  /// \name Accessors
  /// @{
  
  decltype(FragmentMap) const &getFragmentMap() const { return FragmentMap; }
  
  std::size_t getNumberOfFragments() const { return FragmentMap.size(); }
  
  /// @} (Accessors)
  
  
  /// \name Mutators
  /// @{
  
  void add(MappedMemoryBlock Block, EventLocation Event);
  
  void clear(MemoryArea Area);
  
  /// @} (Mutators)
};

/// Print a textual description of a MemoryState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MemoryState const &State);


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_MEMORYSTATE_HPP
