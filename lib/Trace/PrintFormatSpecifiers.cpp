//===- lib/Trace/PrintFormatSpecifiers.cpp --------------------------------===//
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

#include "seec/Trace/PrintFormatSpecifiers.hpp"

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
  
  if (*Remainder == '\0')
    return Result;
  
  // Read width.
  if (*Remainder == '*') {
    Result.WidthSpecified = true;
    Result.WidthAsArgument = true;
    if (*++Remainder == '\0')
      return Result;
  }
  else if(isdigit(*Remainder)){
    Result.WidthSpecified = true;
    
    // Parse int from string.
    char *ParseEnd;
    Result.Width = std::strtoul(Remainder, &ParseEnd, 10);
    Remainder = ParseEnd;
    
    if (*Remainder == '\0')
      return Result;
  }
  
  // Read precision.
  if (*Remainder == '.') {
    Result.PrecisionSpecified = true;
    if (*++Remainder == '\0')
      return Result;
    
    if (*Remainder == '*') {
      Result.PrecisionAsArgument = true;
      if (*++Remainder == '\0')
        return Result;
    }
    else if(isdigit(*Remainder)) {
      // Parse int from string. If it does not exist, then the precision is
      // defined to be zero, which is what strtoul will return on failure.
      char *ParseEnd;
      Result.Precision = std::strtoul(Remainder, &ParseEnd, 10);
      Remainder = ParseEnd;
      
      if (*Remainder == '\0')
        return Result;
    }
  }
  
  // Read length modifier.
  Result.Length = readLengthModifier(Remainder, &Remainder);
  if (*Remainder == '\0')
    return Result;
  
  // Read specifier and set the default precision is no precision was
  // specified.
  switch (*Remainder) {
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS)  \
    case CHR:                                                                  \
      Result.Conversion = Specifier::ID;                                       \
      if (!Result.PrecisionSpecified && PREC)                                  \
        Result.Precision = DPREC;                                              \
      break;
#include "seec/Trace/PrintFormatSpecifiers.def"
    default:
      return Result;
  }
  
  // Set the end of the parsed region.
  Result.End = ++Remainder;
  
  return Result;
}


} // namespace trace (in seec)

} // namespace seec
