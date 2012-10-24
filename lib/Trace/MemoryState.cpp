#include "seec/Trace/MemoryState.hpp"
#include "seec/Util/Printing.hpp"

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// MemoryState Mutators
//------------------------------------------------------------------------------

void MemoryState::add(MappedMemoryBlock Block, EventLocation Event) {
  // Clear space for the new fragment.
  clear(Block.area());
  
  // Add the new fragment.
  auto const Address = Block.start();
  
  // TODO: get an iterator from clear() to hint to insert.
  FragmentMap.insert(std::make_pair(Address,
                                    MemoryStateFragment(std::move(Block),
                                                        std::move(Event))));
}

void MemoryState::clear(MemoryArea const Area) {
  // Convenience variables.
  auto const Address = Area.start();
  auto const LastAddress = Area.lastAddress();
  
  // Get the first fragment starting >= Address.
  auto It = FragmentMap.lower_bound(Address);

  // Best-case scenario: perfect removal of a previous state.
  if (It != FragmentMap.end() && It->second.getBlock().area() == Area) {
    FragmentMap.erase(It);
    return;
  }

  // Check if the previous fragment overlaps.
  if (It->first > Address && It != FragmentMap.begin()) {
    if ((--It)->second.getBlock().lastAddress() >= Area.start()) {
      // Previous fragment overlaps with our start. Check if we are splitting
      // the fragment or performing a right-trim.
      if (It->second.getBlock().lastAddress() > LastAddress) { // Split
        // Create a new fragment for the right-hand side.
        MappedMemoryBlock RightBlock(It->second.getBlock());
        RightBlock.trimLeftSide(LastAddress + 1);
        MemoryStateFragment RightFragment(std::move(RightBlock),
                                          It->second.getStateRecordLocation());
        
        // Resize the previous fragment to remove the right-hand and overlap.
        It->second.getBlock().setEnd(Address);
        
        // Insert the right-hand side fragment.
        // TODO: Hint this insertion.
        FragmentMap.insert(std::make_pair(LastAddress + 1,
                                          std::move(RightFragment)));
      }
      else { // Right-trim
        // Resize the previous fragment to remove the overlapping area.
        It->second.getBlock().setEnd(Area.start());
      }
    }

    ++It;
  }

  // Find and remove overlapping fragments.
  while (It != FragmentMap.end() && It->first <= LastAddress) {
    if (It->second.getBlock().lastAddress() <= LastAddress) {
      // Remove completely replaced fragment.
      FragmentMap.erase(It++);
    }
    else {
      // Reposition right-overlapping fragment.
      auto Fragment = std::move(It->second);
      FragmentMap.erase(It++);
      Fragment.getBlock().trimLeftSide(LastAddress + 1);
      FragmentMap.insert(It,
                         std::make_pair(LastAddress + 1, std::move(Fragment)));
      break;
    }
  }
}

void MemoryState::unsplit(uintptr_t LeftAddress, uintptr_t RightAddress) {
 auto LeftIt = FragmentMap.find(LeftAddress);
 auto RightIt = FragmentMap.find(RightAddress);
 assert(LeftIt != FragmentMap.end() && RightIt != FragmentMap.end());
 
 // Set the left fragment to cover the entire (merged) range.
 LeftIt->second.getBlock().setEnd(RightIt->second.getBlock().end());
 
 // Delete the right fragment.
 FragmentMap.erase(RightIt);
}

void MemoryState::untrimRightSide(uintptr_t Address, std::size_t TrimSize) {
  auto It = FragmentMap.find(Address);
  assert(It != FragmentMap.end() && "Illegal MemoryState::untrimRightSide.");
  
  // Increase the size of the fragment.
  It->second.getBlock().setEnd(It->second.getBlock().end() + TrimSize);
}

void MemoryState::untrimLeftSide(uintptr_t Address, uintptr_t PriorAddress) {
  auto It = FragmentMap.find(Address);
  assert(It != FragmentMap.end() && "Illegal MemoryState::untrimLeftSide.");
  
  // Shift the fragment back to its original position.
  auto Fragment = std::move(It->second);
  FragmentMap.erase(It++);
  Fragment.getBlock().untrimLeftSide(PriorAddress);
  FragmentMap.insert(It, std::make_pair(PriorAddress, std::move(Fragment)));
}


//------------------------------------------------------------------------------
// MemoryState::Region
//------------------------------------------------------------------------------

bool MemoryState::Region::isCompletelyInitialized() const {
  auto It = State.FragmentMap.lower_bound(Area.start());
  
  // Check if the fragment completely covers our area.
  if (It != State.FragmentMap.end()
      && It->second.getBlock().area().contains(Area)) {
      return true;
  }
  
  auto CoveredPriorTo = Area.start();
  
  // If this fragment does not line up with the start of our area, then check
  // the previous fragment to see if it covers the start of our area.
  if (It == State.FragmentMap.end() || It->first > Area.start()) {
    // If there's no previous fragment, our start is not covered.
    if (It == State.FragmentMap.begin())
      return false;
    
    --It;
    
    CoveredPriorTo = It->second.getBlock().area().end();
    if (CoveredPriorTo >= Area.end())
      return true;
    
    ++It;
  }
  
  for (; It != State.FragmentMap.end(); ++It) {
    // Check if there was a gap between this fragment and the last fragment (if
    // we didn't check the previous fragment above, then the first time through
    // the loop this must be false, as It->first will equal Area.start()).
    if (It->first != CoveredPriorTo)
      return false;
    
    CoveredPriorTo = It->second.getBlock().area().end();
    
    // This fragment covers the remainder of our area.
    if (CoveredPriorTo >= Area.end())
      return true;
  }
  
  // There were not enough fragments to cover our area.
  return false;
}

std::vector<char> MemoryState::Region::getByteInitialization() const {
  auto const Start = Area.start();
  auto const Length = Area.length();
  
  // Set all bytes to zero (uninitialized).
  std::vector<char> Initialization(Length, 0);

  auto It = State.FragmentMap.lower_bound(Start);

  // Best-case scenario: this block's state was set in a single fragment.
  if (It != State.FragmentMap.end()
      && It->second.getBlock().area().contains(Area)) {
    memset(Initialization.data(), 0xFF, Area.length());
    return Initialization;
  }

  // Check if the previous fragment overlaps our area.
  if ((It == State.FragmentMap.end() || It->first > Start)
      && It != State.FragmentMap.begin()) {
    --It;
    
    auto const FragmentEnd = It->second.getBlock().area().end();
    if (FragmentEnd > Start) {
      auto Covered = FragmentEnd - Start;
      
      if (Covered > Length)
        Covered = Length;
      
      memset(Initialization.data(), 0xFF, Covered);
      
      if (Covered == Length)
        return Initialization;
    }
    
    ++It;
  }
  
  // Find and set the values for overlapping fragments.
  for (; It != State.FragmentMap.end() && It->first < Area.end(); ++It) {
    // Offset of the fragment start in our area.
    auto const FragmentOffset = It->first - Start;
    
    auto const FragmentEnd = It->second.getBlock().area().end();
    auto const FragmentLength = FragmentEnd - It->first;
    
    // Determine how much of the fragment fits into our area.
    auto const Covered = (FragmentLength <= (Length - FragmentOffset))
                       ? (FragmentLength)
                       : (Length - FragmentOffset);
    
    memset(Initialization.data() + FragmentOffset, 0xFF, Covered);
  }

  return Initialization;
}

std::vector<char> MemoryState::Region::getByteValues() const {
  auto const Start = Area.start();
  auto const Length = Area.length();
  
  // Set all bytes to zero (uninitialized).
  std::vector<char> Values(Length, 0);

  auto It = State.FragmentMap.lower_bound(Start);

  // Best-case scenario: this block's state was set in a single fragment.
  if (It != State.FragmentMap.end()
      && It->second.getBlock().area().contains(Area)) {
    // Copy data from the fragment.
    std::memcpy(Values.data(), It->second.getBlock().data(), Area.length());
    return Values;
  }

  // Check if the previous fragment overlaps our area.
  if ((It == State.FragmentMap.end() || It->first > Start)
      && It != State.FragmentMap.begin()) {
    --It;
    
    auto const FragmentEnd = It->second.getBlock().area().end();
    if (FragmentEnd > Start) {
      // Find the offset of our start position in the fragment.
      auto const OffsetInFragment = Start - It->first;
      
      // Find out how many bytes of the fragment fit into our area.
      auto const Covered = ((FragmentEnd - Start) <= Length)
                         ? (FragmentEnd - Start)
                         : Length;
      
      // Copy data from the fragment.
      std::memcpy(Values.data(),
                  It->second.getBlock().data() + OffsetInFragment,
                  Covered);
      
      if (Covered == Length)
        return Values;
    }
    
    ++It;
  }
  
  // Find and set the values for overlapping fragments.
  for (; It != State.FragmentMap.end() && It->first < Area.end(); ++It) {
    // Offset of the fragment start in our area.
    auto const FragmentOffset = It->first - Start;
    
    auto const FragmentEnd = It->second.getBlock().area().end();
    auto const FragmentLength = FragmentEnd - It->first;
    
    // Determine how much of the fragment fits into our area.
    auto const Covered = (FragmentLength <= (Length - FragmentOffset))
                       ? (FragmentLength)
                       : (Length - FragmentOffset);
    
    std::memcpy(Values.data() + FragmentOffset,
                It->second.getBlock().data(),
                Covered);
  }

  return Values;
}


//------------------------------------------------------------------------------
// MemoryState Printing
//------------------------------------------------------------------------------

llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MemoryState const &State) {
  Out << " MemoryState:\n";

  for (auto const &Fragment : State.getFragmentMap()) {
    auto const &Block = Fragment.second.getBlock();
    Out << "  [" << Block.start() << ", " << Block.end() << ")";

    if (Block.length() <= 8) {
      Out << ": ";
      seec::util::write_hex_bytes(Out, Block.data(), Block.length());
    }

    Out << "\n";
  }

  return Out;
}


} // namespace trace (in seec)

} // namespace seec
