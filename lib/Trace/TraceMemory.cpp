//===- lib/Trace/TraceMemory.cpp ------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Trace/TraceMemory.hpp"
#include "seec/Util/Maybe.hpp"

namespace seec {

namespace trace {

OverwrittenMemoryInfo
TraceMemoryState::add(uint64_t Address,
                      uint64_t Length,
                      uint32_t ThreadID,
                      offset_uint StateRecordOffset,
                      uint64_t ProcessTime) {
  auto LastAddress = Address + (Length - 1);
  
  // collect overwritten states
  OverwrittenMemoryInfo Overwritten;

  // get the first fragment starting >= Address.
  auto It = Fragments.lower_bound(Address);

  // best-case scenario: perfect replacement of a previous state
  if (It->first == Address && It->second.area().lastAddress() == LastAddress) {
    Overwritten.add(It->second);
    
    // move It to the next state (to use as a placement hint for the new state)
    // and delete the overwritten state
    Fragments.erase(It++);
    
    Fragments.insert(It, std::make_pair(Address,
                                        TraceMemoryFragment(Address,
                                                            Length,
                                                            ThreadID,
                                                            StateRecordOffset,
                                                            ProcessTime)));

    return Overwritten;
  }

  // Check if the previous fragment overlaps.
  if (It->first > Address && It != Fragments.begin()) {
    if ((--It)->second.area().lastAddress() >= Address) {
      Overwritten.add(It->second);
      
      // Resize the fragment to remove the overlapping area.
      It->second.area().setEnd(Address);
    }
    
    ++It;
  }

  // find remaining overlapping fragments
  while (It != Fragments.end() && It->first <= LastAddress) {
    Overwritten.add(It->second);
    
    if (It->second.area().lastAddress() <= LastAddress) {
      // remove internally overlapping fragment
      Fragments.erase(It++);
    }
    else {
      // reposition right-overlapping fragment
      auto Fragment = std::move(It->second);
      Fragments.erase(It++);
      Fragments.insert(It,
                       std::make_pair(LastAddress + 1, std::move(Fragment)));
      break;
    }
  }

  // add the new fragment.
  // Fragments.emplace(Address, Address, Length, StateRecordOffset);
  Fragments.insert(It,
                   std::make_pair(Address,
                                  TraceMemoryFragment(Address,
                                                      Length,
                                                      ThreadID,
                                                      StateRecordOffset,
                                                      ProcessTime)));

  return Overwritten;
}

OverwrittenMemoryInfo TraceMemoryState::clear(uint64_t Address, 
                                              uint64_t Length) {
  auto LastAddress = Address + (Length - 1);
  
  // Collect overwritten states.
  OverwrittenMemoryInfo Overwritten;

  // Get the first fragment starting >= Address.
  auto It = Fragments.lower_bound(Address);

  // Best-case scenario: perfect removal of a previous state.
  if (It->first == Address && It->second.area().lastAddress() == LastAddress) {
    Overwritten.add(It->second);
    
    Fragments.erase(It);

    return Overwritten;
  }

  // Check if the previous fragment overlaps.
  if (It->first > Address && It != Fragments.begin()) {
    if ((--It)->second.area().lastAddress() >= Address) {
      Overwritten.add(It->second);
      
      // Trim the length of the previous fragment.
      auto FragmentAddress = It->second.area().address();
      auto NewFragmentLength = Address - FragmentAddress;
      
      It->second.reposition(MemoryArea{FragmentAddress, NewFragmentLength});
    }
    
    ++It;
  }

  // Find remaining overlapping fragments.
  while (It != Fragments.end() && It->first <= LastAddress) {
    Overwritten.add(It->second);
    
    if (It->second.area().lastAddress() <= LastAddress) {
      // Remove internally overlapping fragment.
      Fragments.erase(It++);
    }
    else {
      // Reposition right-overlapping fragment.
      auto Fragment = std::move(It->second);
      Fragments.erase(It++);
      Fragments.insert(It,
                       std::make_pair(LastAddress + 1, std::move(Fragment)));
      break;
    }
  }

  return Overwritten;
}

bool TraceMemoryState::hasKnownState(uint64_t Address, uint64_t Length) const {
  auto LastAddress = Address + (Length - 1);
  
  auto It = Fragments.lower_bound(Address);
  
  if (It == Fragments.end())
    return false;

  if (It->first == Address && It->second.area().lastAddress() >= LastAddress)
    return true;
    
  if (It == Fragments.begin())
    return false;
  
  if ((--It)->second.area().lastAddress() >= LastAddress)
    return true;
  
  return false;
}

} // namespace trace (in seec)

} // namespace seec
