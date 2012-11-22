//===- Util/ConstExprCString.hpp ------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
// Much of this derives from Andrzej's C++ blog:
//   http://akrzemi1.wordpress.com/2011/05/11/parsing-strings-at-compile-time-part-i/
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_CONSTEXPRCSTRING_HPP
#define SEEC_UTIL_CONSTEXPRCSTRING_HPP

namespace seec {
  
/// Functions for performing constexpr c-string operations.
namespace const_strings {

class StringRef {
  char const * Begin;
  
  unsigned Size;
  
public:
  template<unsigned N>
  constexpr StringRef(char const(&Str)[N])
  : Begin(Str),
    Size(N - 1)
  {
    static_assert(N >= 1, "not a string literal");
  }
  
  constexpr char operator[](unsigned i) {
    return Begin[i];
  }
  
  constexpr operator char const *() {
    return Begin;
  }
  
  constexpr unsigned size() {
    return Size;
  }
};

constexpr bool contains(StringRef Str, char c, unsigned i) {
  return i >= Str.size() ? false
                         : Str[i] == c ? true
                                       : contains(Str, c, i + 1);
}

constexpr bool contains(StringRef Str, char c) {
  return contains(Str, c, 0);
}

} // namespace const_strings (in seec)

} // namespace seec

#endif // SEEC_UTIL_CONSTEXPRCSTRING_HPP
