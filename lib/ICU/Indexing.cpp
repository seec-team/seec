//===- lib/ICU/Indexing.cpp ----------------------------------------- C++ -===//
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


#include "seec/ICU/Indexing.hpp"
#include "seec/ICU/Output.hpp"

#include "llvm/Support/raw_ostream.h"

#include <vector>


namespace seec {

namespace icu {


seec::util::Maybe<IndexedString>
IndexedString::from(UnicodeString const &String)
{
  if (String.isBogus())
    return seec::util::Maybe<IndexedString>();
  
  UnicodeString const NeedleStart("@[");
  UnicodeString const NeedleEscape("@[[");
  UnicodeString const NeedleEnd("]");
  
  UnicodeString CleanedString; // String with index indicators removed.
  std::multimap<UnicodeString, Needle> Needles;
  
  std::vector<std::pair<UnicodeString, uint32_t>> IndexStack;
  
  uint32_t SearchFrom = 0; // Current offset in String.
  uint32_t FoundStart; // Position of matched index indicator.
  
  while ((FoundStart = String.indexOf(NeedleStart, SearchFrom)) != -1) {
    // Copy all the literal string data.
    CleanedString.append(String, SearchFrom, FoundStart - SearchFrom);
    
    // Check if this is an escape sequence.
    if (String.compare(FoundStart, NeedleEscape.length(), NeedleEscape) == 0) {
      CleanedString.append(NeedleStart);
      SearchFrom = FoundStart + NeedleEscape.length();
      continue;
    }
    
    // Find the end of this sequence.
    uint32_t FoundEnd = String.indexOf(NeedleEnd, SearchFrom);
    if (FoundEnd == -1)
      return seec::util::Maybe<IndexedString>();
    
    if (FoundEnd == FoundStart + NeedleStart.length()) {
      // This is a closing sequence.
      if (IndexStack.size() == 0)
        return seec::util::Maybe<IndexedString>();
      
      // Pop the starting details of the last-opened sequence.
      auto Start = IndexStack.back();
      IndexStack.pop_back();
      
      // Store the needle for this sequence.
      Needles.insert(std::make_pair(Start.first,
                                    Needle(Start.second,
                                           CleanedString.countChar32())));
    }
    else {
      // This is an opening sequence.
      uint32_t const NameStart = FoundStart + NeedleStart.length();
      uint32_t const NameLength = FoundEnd - NameStart;
      
      IndexStack.emplace_back(UnicodeString(String, NameStart, NameLength),
                              CleanedString.countChar32());
    }
    
    SearchFrom = FoundEnd + NeedleEnd.length();
  }
  
  // Copy all remaining literal data.
  CleanedString.append(String, SearchFrom, String.length() - SearchFrom);
  
  return IndexedString(std::move(CleanedString), std::move(Needles));
}

decltype(IndexedString::Needles)::const_iterator
IndexedString::lookupPrimaryIndexAtCharacter(int32_t Position) const
{
  auto Result = Needles.end();
  int32_t ResultEnd = std::numeric_limits<int32_t>::max();
  
  for (auto It = Needles.begin(); It != Needles.end(); ++It) {
    if (It->second.getStart() <= Position
        && Position < It->second.getEnd()
        && It->second.getEnd() < ResultEnd)
    {
      Result = It;
      ResultEnd = It->second.getEnd();
    }
  }
  
  return Result;
}

std::vector<UnicodeString>
IndexedString::getIndicesAtCharacter(int32_t Position) const
{
  std::vector<UnicodeString> Result;
  
  for (auto const &Pair : Needles) {
    if (Pair.second.getStart() <= Position && Position < Pair.second.getEnd()) {
      Result.emplace_back(Pair.first);
    }
  }
  
  return Result;
}

UnicodeString
IndexedString::getPrimaryIndexAtCharacter(int32_t Position) const
{
  auto const Found = lookupPrimaryIndexAtCharacter(Position);
  return Found != Needles.end() ? Found->first : UnicodeString();
}


} // namespace icu (in seec)

} // namespace seec
