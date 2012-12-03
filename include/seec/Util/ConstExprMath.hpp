//===- Util/ConstExprMath.hpp --------------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_CONSTEXPRMATH_HPP
#define SEEC_UTIL_CONSTEXPRMATH_HPP

namespace seec {
  
/// Functions for performing constexpr math operations.
namespace const_math {
  
template<typename T>
constexpr T max(T Only) {
  return Only;
}

template<typename T>
constexpr T max(T Left, T Right) {
  return Left < Right ? Right : Left;
}

template<typename T1, typename T2, typename... TS>
constexpr T1 max(T1 Left, T2 Right, TS... Remaining) {
  return sizeof...(Remaining)
          ? max(max(Left, Right), Remaining...)
          : (Left < Right ? Right : Left);
}

} // namespace const_math (in seec)

} // namespace seec

#endif // SEEC_UTIL_CONSTEXPRMATH_HPP
