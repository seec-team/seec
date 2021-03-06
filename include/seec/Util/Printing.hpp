//===- seec/Util/Printing.hpp --------------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_PRINTING_HPP
#define SEEC_UTIL_PRINTING_HPP

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cstdint>
#include <string>

namespace llvm {
  class raw_ostream;
}

namespace seec {

namespace util {

/// \name Write hex bytes to an output stream.
/// @{

inline
void write_hex_byte(llvm::raw_ostream &Out, unsigned char Byte) {
  char const High = static_cast<char>(Byte >> 4);
  char const Low = static_cast<char>(Byte & 0xF);

  Out.write((High < 10) ? ('0' + High) : ('a' + (High - 10)));
  Out.write((Low < 10) ? ('0' + Low) : ('a' + (Low - 10)));
}

inline
void write_hex_bytes(llvm::raw_ostream &Out, unsigned char const *Start,
                     size_t Length) {
  for (auto End = Start + Length; Start != End; ++Start) {
    write_hex_byte(Out, *Start);
  }
}

inline
void write_hex_bytes(llvm::raw_ostream &Out, signed char const *Start,
                     size_t Length) {
  write_hex_bytes(Out, reinterpret_cast<unsigned char const *>(Start), Length);
}

inline
void write_hex_bytes(llvm::raw_ostream &Out, char const *Start,
                     size_t Length) {
  write_hex_bytes(Out, reinterpret_cast<unsigned char const *>(Start), Length);
}

template<typename T>
void write_hex_padded(llvm::raw_ostream &Out, T Value) {
  Out << "0x";

  auto Shift = 8 * (sizeof(T) - 1);

  for (unsigned i = 0; i < sizeof(T); ++i) {
    write_hex_byte(Out, (Value >> Shift) & 0xFF);
    Shift -= 8;
  }
}

/// @}


/// \name Convert values to strings containing their hex representation.
/// @{

/// \brief Get a string with the hex representation of the given byte.
inline
std::string to_hex_string(unsigned char Byte) {
  char const High = static_cast<char>(Byte >> 4);
  char const Low = static_cast<char>(Byte & 0xF);

  return std::string {
    static_cast<char>((High < 10) ? ('0' + High) : ('a' + (High - 10))),
    static_cast<char>((Low < 10) ? ('0' + Low) : ('a' + (Low - 10)))
    };
}

/// \brief Get a string with the hex representation of the given byte.
inline
std::string to_hex_string(signed char Byte) {
  return to_hex_string(static_cast<unsigned char>(Byte));
}

/// \brief Get a string with the hex representation of the given byte.
inline
std::string to_hex_string(char Byte) {
  return to_hex_string(static_cast<unsigned char>(Byte));
}

/// @}


/// \name Convert strings to JSON string literals.
/// @{

/// \brief Write the contents of S as a JSON string literal to Out.
///
void writeJSONStringLiteral(llvm::StringRef S, llvm::raw_ostream &Out);

/// @} (Convert strings to JSON string literals.)


/// \brief Track indentation, to assist printing structured information.
///
class IndentationGuide
{
  /// The character (or string) used for each step of indentation.
  std::string const Character;
  
  /// Amount of times to repeat the IndentCharacter per step of indentation.
  std::size_t const Step;
  
  /// Steps of indentation.
  std::size_t Indentation;
  
  /// The current indentation.
  std::string IndentationString;
  
public:
  /// \brief Constructor.
  ///
  IndentationGuide()
  : Character(),
    Step(0),
    Indentation(0),
    IndentationString()
  {}
  
  /// \brief Constructor.
  ///
  IndentationGuide(std::string WithCharacter)
  : Character(std::move(WithCharacter)),
    Step(1),
    Indentation(0),
    IndentationString()
  {}
  
  /// \brief Constructor.
  ///
  IndentationGuide(std::string WithCharacter, std::size_t const WithStep)
  : Character(std::move(WithCharacter)),
    Step(WithStep),
    Indentation(0),
    IndentationString()
  {}
  
  /// \brief Add a level of indentation.
  ///
  std::size_t indent() {
    ++Indentation;
    
    for (std::size_t i = 0; i < Step; ++i)
      IndentationString.append(Character);
    
    return Indentation;
  }
  
  /// \brief Remove a level of indentation.
  ///
  std::size_t unindent() {
    if (!Indentation)
      return Indentation;
    
    --Indentation;
    
    auto const NumCharsToRemove = Character.size() * Step;
    auto const Size = IndentationString.size();
    assert(Size >= NumCharsToRemove);
    
    IndentationString.resize(Size - NumCharsToRemove);
    
    return Indentation;
  }
  
  /// \brief Get the amount of indentation.
  ///
  std::size_t getIndentation() const { return Indentation; }
  
  /// \brief Get the indentation string.
  ///
  std::string const &getString() const { return IndentationString; }
};


} // namespace util

} // namespace seec

#endif // _SEEC_UTIL_PRINTING_HPP_
