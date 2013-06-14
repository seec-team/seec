//===- lib/ICU/LineWrapper.cpp --------------------------------------------===//
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

#include "seec/ICU/LineWrapper.hpp"

#include <memory>


namespace seec {


std::vector<WrappedLine>
wrapParagraph(BreakIterator &Breaker,
              UnicodeString const &Paragraph,
              std::function<bool (UnicodeString const &Line)> LengthCheck)
{
  std::vector<WrappedLine> Wrapped;
  
  Breaker.setText(Paragraph);
  
  auto const Length = Paragraph.length();
  
  int32_t Start = 0;
  int32_t End = 0;
  
  while (Start < Length) {
    // Try to add the next character.
    if ((End = Breaker.next()) == BreakIterator::DONE)
      break;
    
    // If the length of this line is OK, then keep adding characters.
    if (LengthCheck(Paragraph.tempSubStringBetween(Start, End)))
      continue;
    
    // We need to wind back to the last legal position and finish this line.
    End = Breaker.previous();
    
    // If the last legal position was the start, then the line has no legal
    // break position for this length, so we need to add by character.
    if (Start == End) {
      for (++End; End < Length; ++End) {
        if (!LengthCheck(Paragraph.tempSubStringBetween(Start, End))) {
          --End;
          break;
        }
      }
      
      // If we can't even fit one character, we'll just have to put a character
      // in anyway.
      if (Start == End)
        ++End;
    }
    
    // Find how many trailing whitespace characters there are.
    int32_t NonWhitespace = End - 1;
  
    for (; NonWhitespace > Start; --NonWhitespace) {
      auto const Char = Paragraph[NonWhitespace];
      if (!u_isspace(Char)
          && u_charType(Char) != U_CONTROL_CHAR
          && u_charType(Char) != U_NON_SPACING_MARK)
        break;
    }
    
    Wrapped.push_back({Start, End, End - (NonWhitespace + 1)});
    
    Start = End;
  }
  
  // Add the final line.
  if (Start < Length) {
    int32_t NonWhitespace = Length - 1;
    
    for (; NonWhitespace > Start; --NonWhitespace) {
      auto const Char = Paragraph[NonWhitespace];
      if (!u_isspace(Char)
          && u_charType(Char) != U_CONTROL_CHAR
          && u_charType(Char) != U_NON_SPACING_MARK)
        break;
    }
    
    Wrapped.push_back({Start, Length, Length - (NonWhitespace + 1)});
  }
  
  return Wrapped;
}


} // namespace seec
