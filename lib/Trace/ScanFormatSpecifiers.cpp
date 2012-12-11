//===- lib/Trace/ScanFormatSpecifiers.cpp ---------------------------------===//
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

#include "seec/Trace/ScanFormatSpecifiers.hpp"

namespace seec {

namespace trace {

ScanConversionSpecifier
ScanConversionSpecifier::readNextFrom(char const * const String) {
  ScanConversionSpecifier Result;
  
  // Find '%'.
  Result.Start = std::strchr(String, '%');
  if (!Result.Start)
    return Result;
  
  auto Remainder = Result.Start;
  if (*++Remainder == '\0')
    return Result;
  
  // Read assignment suppression '*'.
  if (*Remainder == '*') {
    Result.SuppressAssignment = true;
    if (*++Remainder == '\0')
      return Result;
  }
  
  // Read maximum field width.
  if(isdigit(*Remainder)){
    Result.WidthSpecified = true;
    
    // Parse int from string.
    char *ParseEnd;
    Result.Width = std::strtoul(Remainder, &ParseEnd, 10);
    Remainder = ParseEnd;
    
    if (*Remainder == '\0')
      return Result;
  }
  
  // Read length modifier.
  Result.Length = readLengthModifier(Remainder, &Remainder);
  if (*Remainder == '\0')
    return Result;
    
  // Read specifier.
  switch (*Remainder) {
#define SEEC_SCAN_FORMAT_SPECIFIER(ID, CHR, SUPPRESS, LENS)                    \
    case CHR:                                                                  \
      Result.Conversion = Specifier::ID;                                       \
      break;
#include "seec/Trace/ScanFormatSpecifiers.def"
    default:
      return Result;
  }
  
  // Special case for parsing set specifier.
  if (Result.Conversion == Specifier::set) {
    if (*++Remainder == '\0')
      return Result;
    
    // Make set a negation.
    if (*Remainder == '^') {
      Result.SetNegation = true;
      if (*++Remainder == '\0')
        return Result;
    }
    
    // Special case for including ']' in a set.
    if (*Remainder == ']') {
      Result.SetCharacters.push_back(']');
      if (*++Remainder == '\0')
        return Result;
    }
    
    // Add all remaining characters to the set (until ']' is found).
    while (*Remainder != ']') {
      Result.SetCharacters.push_back(*Remainder);
      if (*++Remainder == '\0')
        return Result;
    }
  }
  
  // Set the end of the parsed region.
  Result.End = ++Remainder;
  
  return Result;
}


} // namespace trace (in seec)

} // namespace seec
