//===- include/seec/ICU/Indexing.hpp -------------------------------- C++ -===//
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

#ifndef SEEC_ICU_INDEXING_HPP
#define SEEC_ICU_INDEXING_HPP


#include "seec/Util/Maybe.hpp"

#include "unicode/unistr.h"

#include <cstdint>
#include <map>
#include <vector>


namespace seec {

namespace icu {


/// \brief Represents a range in a string.
///
class Needle {
  /// Index of the first character in this range.
  int32_t Start;
  
  /// Index of the first character following this range.
  int32_t End;
  
public:
  /// \name Constructors
  /// @{
  
  /// \brief Construct a new Needle.
  ///
  Needle(int32_t TheStart, int32_t TheEnd)
  : Start(TheStart),
    End(TheEnd)
  {}
  
  /// \brief Copy an existing Needle.
  ///
  Needle(Needle const &) = default;
  
  /// @}
  
  
  /// \name Accessors
  /// @{
  
  int32_t getStart() const { return Start; }
  
  int32_t getEnd() const { return End; }
  
  int32_t getLength() const { return End - Start; }
  
  /// @}
};


///
///
class IndexedString {
  /// The string (with index indicators removed).
  UnicodeString String;
  
  /// Lookup for all indexing Needles.
  std::multimap<UnicodeString, Needle> Needles;
  
  /// \brief Constructor.
  ///
  IndexedString(UnicodeString TheString,
                std::multimap<UnicodeString, Needle> TheNeedles)
  : String(std::move(TheString)),
    Needles(std::move(TheNeedles))
  {}
  
public:
  /// \brief Copy constructor.
  IndexedString(IndexedString const &) = default;
  
  /// \brief Move constructor.
  IndexedString(IndexedString &&Other)
  : String(Other.String),
    Needles(std::move(Other.Needles))
  {}
  
  static seec::util::Maybe<IndexedString> from(UnicodeString const &String);
  
  /// \name Accessors
  /// @{
  
  UnicodeString const &getString() const { return String; }
  
  decltype(Needles) const &getNeedleLookup() const { return Needles; }
  
  std::vector<UnicodeString> getIndicesAtCharacter(int32_t Position) const;
  
  /// @}
};


}

}

#endif // SEEC_ICU_INDEXING_HPP
