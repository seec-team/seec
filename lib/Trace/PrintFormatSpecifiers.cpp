#include "PrintFormatSpecifiers.hpp"

namespace seec {

namespace trace {

PrintConversionSpecifier
PrintConversionSpecifier::readNextFrom(char const * const String) {
  PrintConversionSpecifier Result;
  
  // Find '%'.
  Result.Start = std::strchr(String, '%');
  if (!Result.Start)
    return Result;
  
  auto Remainder = Result.Start;
  
  // Read flags.
  while (*++Remainder) { // Get the next non-null character.
    switch (*Remainder) {
      case '-':
        Result.JustifyLeft = true;
        continue;
      case '+':
        Result.SignAlwaysPrint = true;
        continue;
      case ' ':
        Result.SignPrintSpace = true;
        continue;
      case '#':
        Result.AlternativeForm = true;
        continue;
      case '0':
        Result.PadWithZero = true;
        continue;
      default:
        break;
    }
    
    break; // Not a flag, so exit the flag-reading loop.
  }
  
  // Read width.
  if (*Remainder == '*') {
    Result.WidthSpecified = true;
    Result.WidthAsArgument = true;
    ++Remainder;
  }
  else if(isdigit(*Remainder)){
    Result.WidthSpecified = true;
    
    // Parse int from string.
    char *ParseEnd;
    Result.Width = std::strtoul(Remainder, &ParseEnd, 10);
    Remainder = ParseEnd;
  }
  
  // Read precision.
  if (*Remainder == '.') {
    Result.PrecisionSpecified = true;
    ++Remainder;
    
    if (*Remainder == '*') {
      Result.PrecisionAsArgument = true;
      ++Remainder;
    }
    else if(isdigit(*Remainder)) {
      // Parse int from string. If it does not exist, then the precision is
      // defined to be zero, which is what strtoul will return on failure.
      char *ParseEnd;
      Result.Precision = std::strtoul(Remainder, &ParseEnd, 10);
      Remainder = ParseEnd;
    }
  }
  
  // Read length modifier.
  switch (*Remainder) {
    case 'h':
      if (*++Remainder == 'h') {
        Result.Length = LengthModifier::hh;
        ++Remainder;
      }
      else {
        Result.Length = LengthModifier::h;
      }
      break;
    case 'l':
      if (*++Remainder == 'l') {
        Result.Length = LengthModifier::ll;
        ++Remainder;
      }
      else {
        Result.Length = LengthModifier::l;
      }
      break;
    case 'j':
      Result.Length = LengthModifier::j;
      ++Remainder;
      break;
    case 'z':
      Result.Length = LengthModifier::z;
      ++Remainder;
      break;
    case 't':
      Result.Length = LengthModifier::t;
      ++Remainder;
      break;
    case 'L':
      Result.Length = LengthModifier::L;
      ++Remainder;
      break;
    default:
      break;
  }
  
  // Read specifier and set the default precision is no precision was
  // specified.
  switch (*Remainder) {
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS)  \
    case CHR:                                                                  \
      Result.Conversion = Specifier::ID;                                       \
      if (!Result.PrecisionSpecified && PREC)                                  \
        Result.Precision = DPREC;                                              \
      break;
#include "PrintFormatSpecifiers.def"
    default:
      return Result;
  }
  
  // Set the end of the parsed region.
  Result.End = ++Remainder;
  
  return Result;
}


} // namespace trace (in seec)

} // namespace seec
