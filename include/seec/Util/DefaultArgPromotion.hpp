//===- Util/DefaultArgPromotion.hpp --------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_DEFAULTARGPROMOTION_HPP
#define SEEC_UTIL_DEFAULTARGPROMOTION_HPP

namespace seec {


/// \brief Get the default argument promotion type of a type.
///
template<typename T>
struct default_arg_promotion_of { typedef T type; };

template<>
struct default_arg_promotion_of<unsigned char> { typedef unsigned type; };

template<>
struct default_arg_promotion_of<signed char> { typedef int type; };

template<>
struct default_arg_promotion_of<unsigned short> { typedef unsigned type; };

template<>
struct default_arg_promotion_of<signed short> { typedef int type; };

template<>
struct default_arg_promotion_of<float> { typedef double type; };


} // namespace seec

#endif // SEEC_UTIL_DEFAULTARGPROMOTION_HPP
