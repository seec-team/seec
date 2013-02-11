//===- Util/ConstExprCString.hpp ------------------------------------ C++ -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Much of this derives from Andrzej's C++ blog:
///   http://akrzemi1.wordpress.com/2011/05/11/parsing-strings-at-compile-time-part-i/
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_CONSTEXPRCSTRING_HPP
#define SEEC_UTIL_CONSTEXPRCSTRING_HPP

namespace seec {
  
/// Functions for performing constexpr c-string operations.
namespace const_strings {


class StringRef {
  char const * const Begin;
  
  unsigned const Size;
  
public:
  constexpr StringRef()
  : Begin(nullptr),
    Size(0)
  {}
  
  constexpr StringRef(char const * const StringBegin, unsigned const StringSize)
  : Begin(StringBegin),
    Size(StringSize)
  {}
  
  template<unsigned N>
  constexpr StringRef(char const(&Str)[N])
  : Begin(Str),
    Size(N - 1)
  {
    static_assert(N >= 1, "not a string literal");
  }
  
  
  /// \name Accessors.
  /// @{
  
  constexpr char operator[](int const i) {
    return Begin[i];
  }
  
  constexpr operator char const *() {
    return Begin;
  }
  
  constexpr unsigned size() {
    return Size;
  }
  
  /// @}
  
  
  /// \name Create mutated copies.
  /// @{
  
  constexpr StringRef takeAllFromIndex(unsigned const i) {
    return i >= size() ? StringRef()
                       : StringRef(Begin + i, size() - i);
  }
  
  /// @}
};


/// \brief Returns true iff the char c is contained in Str at or after index i.
///
constexpr bool contains(StringRef const Str, char const c, unsigned const i) {
  return i >= Str.size() ? false
                         : Str[i] == c ? true
                                       : contains(Str, c, i + 1);
}


/// \brief Returns true iff the char c is contained in Str.
///
constexpr bool contains(StringRef const Str, char const c) {
  return contains(Str, c, 0);
}


/// \brief Returns true iff LHS and RHS have the same contents.
///
constexpr bool operator==(StringRef const LHS, StringRef const RHS) {
  return LHS.size() == 0 ? RHS.size() == 0
                         : LHS[0] != RHS[0] ? false
                                            : LHS.takeAllFromIndex(1)
                                              == RHS.takeAllFromIndex(1);
}


} // namespace const_strings (in seec)

} // namespace seec

#endif // SEEC_UTIL_CONSTEXPRCSTRING_HPP
