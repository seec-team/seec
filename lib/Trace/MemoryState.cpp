#include "seec/Trace/MemoryState.hpp"
#include "seec/Util/Printing.hpp"

namespace seec {

namespace trace {


void MemoryState::add(MappedMemoryBlock Block, EventLocation Event) {
  auto const Address = Block.start();

  // Find the first fragment that starts >= Block's start.
  auto It = FragmentMap.lower_bound(Address);

  // Best-case scenario: perfect replacement of a previous state.
  if (It->second.getBlock().area() == Block.area()) {
    FragmentMap.erase(It++);
    FragmentMap.insert(It,
                       std::make_pair(Address,
                                      MemoryStateFragment(std::move(Block),
                                                          std::move(Event))));
    return;
  }

  // Check if the previous fragment overlaps.
  if (It->first > Address && It != FragmentMap.begin()) {
    if ((--It)->second.getBlock().last() >= Address) {
      // Resize the previous fragment to remove the overlapping area.
      It->second.getBlock().setEnd(Address);
    }

    ++It;
  }

  // Find and remove overlapping fragments.
  auto const LastAddress = Block.last();

  while (It != FragmentMap.end() && It->first <= LastAddress) {
    if (It->second.getBlock().last() <= LastAddress) {
      // Remove completely replaced fragment.
      FragmentMap.erase(It++);
    }
    else {
      // Reposition right-overlapping fragment.
      auto const NewStartAddress = LastAddress + 1;
      auto Fragment = std::move(It->second);
      Fragment.getBlock().trimLeftSide(NewStartAddress);
      FragmentMap.erase(It++);
      FragmentMap.insert(It,
                         std::make_pair(NewStartAddress, std::move(Fragment)));
      break;
    }
  }

  // Add the new fragment.
  FragmentMap.insert(It,
                     std::make_pair(Address,
                                    MemoryStateFragment(std::move(Block),
                                                        std::move(Event))));
}

void MemoryState::clear(MemoryArea Area) {
  auto It = FragmentMap.lower_bound(Area.start());

  // Best-case scenario: perfect removal of a previous state.
  if (It != FragmentMap.end() && It->second.getBlock().area() == Area) {
    FragmentMap.erase(It);
    return;
  }

  // Check if the previous fragment overlaps.
  if (It->first > Area.start() && It != FragmentMap.begin()) {
    if ((--It)->second.getBlock().last() >= Area.start()) {
      // Resize the previous fragment to remove the overlapping area.
      It->second.getBlock().setEnd(Area.start());
    }

    ++It;
  }

  // Find and remove overlapping fragments.
  while (It != FragmentMap.end() && It->first <= Area.last()) {
    if (It->second.getBlock().last() <= Area.last()) {
      // Remove completely replaced fragment.
      FragmentMap.erase(It++);
    }
    else {
      // Reposition right-overlapping fragment.
      auto const NewStartAddress = Area.last() + 1;
      auto Fragment = std::move(It->second);
      Fragment.getBlock().trimLeftSide(NewStartAddress);
      FragmentMap.erase(It++);
      FragmentMap.insert(It,
                         std::make_pair(NewStartAddress, std::move(Fragment)));
      break;
    }
  }
}

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
