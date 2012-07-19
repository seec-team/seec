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
  if (It->second.getBlock().area() == Area) {
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
