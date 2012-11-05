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

OverwrittenMemoryInfo TraceMemoryState::add(uintptr_t Address,
                                            std::size_t Length,
                                            uint32_t ThreadID,
                                            offset_uint StateRecordOffset,
                                            uint64_t ProcessTime) {
  // Make room for the fragment and determine the overwritten memory.
  auto Overwritten = clear(Address, Length);

  // Add the new fragment.
  // TODO: get an iterator from clear() to hint to insert.
  Fragments.insert(std::make_pair(Address,
                                  TraceMemoryFragment(Address,
                                                      Length,
                                                      ThreadID,
                                                      StateRecordOffset,
                                                      ProcessTime)));

  return Overwritten;
}

std::pair<OverwrittenMemoryInfo, std::vector<StateCopy>>
TraceMemoryState::memmove(uintptr_t const Source,
                          uintptr_t const Destination,
                          std::size_t const Size,
                          EventLocation const &Event,
                          uint64_t const ProcessTime) {
  auto const SourceEnd = Source + Size;
  
  // Record the copied fragments.
  std::vector<StateCopy> Copies;
  
  // Create the new fragments.
  decltype(Fragments) Moved;
  auto MovedInsert = Moved.end();
  
  // Get the first source fragment starting >= Source.
  auto It = Fragments.lower_bound(Source);
  
  // Check if the previous fragment overlaps.
  if (It != Fragments.begin()
      && (It == Fragments.end() || It->first > Source)) {
    --It;
    
    if (It->second.area().lastAddress() >= Source) {
      // Previous fragment overlaps with our start.
      if (It->second.area().end() >= SourceEnd) {
        // Fragment completely covers the move area.
        Copies.emplace_back(It->second.stateRecord(), MemoryArea(Source, Size));
        
        MovedInsert =
            Moved.insert(MovedInsert,
                         std::make_pair(Destination,
                                        TraceMemoryFragment(Destination,
                                                            Size,
                                                            Event.getThreadID(),
                                                            Event.getOffset(),
                                                            ProcessTime)));
      }
      else {
        // Copy the right-hand side of the fragment.
        auto const NewSize = It->second.area().withStart(Source).length();
        
        Copies.emplace_back(It->second.stateRecord(),
                            MemoryArea(Source, NewSize));
        
        MovedInsert =
            Moved.insert(MovedInsert,
                         std::make_pair(Destination,
                                        TraceMemoryFragment(Destination,
                                                            NewSize,
                                                            Event.getThreadID(),
                                                            Event.getOffset(),
                                                            ProcessTime)));
      }
    }
    
    ++It;
  }
  
  // Find remaining overlapping fragments.  
  while (It != Fragments.end() && It->first < SourceEnd) {
    auto const NewAddress = Destination + (It->first - Source);
    
    // If this fragment exceeds the move's source range, trim the size.
    auto const NewSize = It->second.area().end() <= SourceEnd
                       ? It->second.area().length()
                       : It->second.area().withEnd(Source + Size).length();
    
    Copies.emplace_back(It->second.stateRecord(),
                        MemoryArea(It->first, NewSize));
    
    MovedInsert =
          Moved.insert(MovedInsert,
                       std::make_pair(NewAddress,
                                      TraceMemoryFragment(NewAddress,
                                                          NewSize,
                                                          Event.getThreadID(),
                                                          Event.getOffset(),
                                                          ProcessTime)));
    
    ++It;
  }
  
  // Make room for the fragments and determine the overwritten memory.
  auto Overwritten = clear(Destination, Size);
  
  // Add the fragments (it would be better if we could move them).
  Fragments.insert(Moved.begin(), Moved.end());
  
  return std::make_pair(Overwritten, Copies);
}

OverwrittenMemoryInfo TraceMemoryState::clear(uintptr_t Address, 
                                              std::size_t Length) {
  auto LastAddress = Address + (Length - 1);
  
  // Collect overwritten states.
  OverwrittenMemoryInfo Overwritten;

  // Get the first fragment starting >= Address.
  auto It = Fragments.lower_bound(Address);

  // Best-case scenario: perfect removal of a previous state.
  if (It != Fragments.end()
      && It->first == Address
      && It->second.area().lastAddress() == LastAddress) {
    Overwritten.add(StateOverwrite::forReplace(It->second));
    
    Fragments.erase(It);

    return Overwritten;
  }

  // Check if the previous fragment overlaps.
  if (It != Fragments.begin()
      && (It == Fragments.end() || It->first > Address)) {
    if ((--It)->second.area().lastAddress() >= Address) {
      // Previous fragment overlaps with our start. Check if we are splitting
      // the fragment or performing a right-trim.
      if (It->second.area().lastAddress() > LastAddress) { // Split
        seec::MemoryArea OverwriteArea(Address, Length);
        Overwritten.add(StateOverwrite::forSplitFragment(It->second,
                                                         OverwriteArea));
        
        // Create a new fragment to use on the right-hand side.
        auto RightFragment = It->second;
        RightFragment.area().setStart(LastAddress + 1);
        
        // Resize the fragment to remove the right-hand side and overlap.
        It->second.area().setEnd(Address);
        
        // Add the right-hand side fragment.
        // TODO: Hint this insertion.
        Fragments.insert(std::make_pair(LastAddress + 1,
                                        std::move(RightFragment)));
      }
      else { // Right-Trim
        Overwritten.add(StateOverwrite::forTrimFragmentRight(It->second,
                                                             Address));
        
        // Resize the fragment to remove the overlapping area.
        It->second.area().setEnd(Address);
      }
    }
    
    ++It;
  }

  // Find remaining overlapping fragments.
  while (It != Fragments.end() && It->first <= LastAddress) {
    if (It->second.area().lastAddress() <= LastAddress) {
      Overwritten.add(StateOverwrite::forReplace(It->second));
      
      // Remove internally overlapping fragment.
      Fragments.erase(It++);
    }
    else {
      Overwritten.add(StateOverwrite::forTrimFragmentLeft(It->second,
                                                          LastAddress + 1));
      
      // Reposition right-overlapping fragment.
      auto Fragment = std::move(It->second);
      Fragments.erase(It++);
      Fragment.area().setStart(LastAddress + 1);
      Fragments.insert(It,
                       std::make_pair(LastAddress + 1, std::move(Fragment)));
      
      break;
    }
  }

  return Overwritten;
}

bool TraceMemoryState::hasKnownState(uintptr_t Address,
                                     std::size_t Length) const {
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
