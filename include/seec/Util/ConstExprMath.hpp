//===- Util/ConstExprMath.hpp --------------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_CONSTEXPRMATH_HPP
#define SEEC_UTIL_CONSTEXPRMATH_HPP

namespace seec {
  
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
