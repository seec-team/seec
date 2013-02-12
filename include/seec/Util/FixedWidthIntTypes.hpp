//===- seec/Util/FixedWidthIntTypes.hpp ----------------------------- C++ -===//
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

#ifndef SEEC_UTIL_FIXEDWIDTHINTTYPES_HPP
#define SEEC_UTIL_FIXEDWIDTHINTTYPES_HPP


#include <cstdint>


namespace seec {


//===----------------------------------------------------------------------===//
// GetInt<N>
//===----------------------------------------------------------------------===//

template<unsigned BitWidth> struct GetInt;

template<> struct GetInt<8>  { typedef int8_t  type; };
template<> struct GetInt<16> { typedef int16_t type; };
template<> struct GetInt<32> { typedef int32_t type; };
template<> struct GetInt<64> { typedef int64_t type; };

//===----------------------------------------------------------------------===//
// GetUInt<N>
//===----------------------------------------------------------------------===//

template<unsigned BitWidth> struct GetUInt;

template<> struct GetUInt<8>  { typedef uint8_t  type; };
template<> struct GetUInt<16> { typedef uint16_t type; };
template<> struct GetUInt<32> { typedef uint32_t type; };
template<> struct GetUInt<64> { typedef uint64_t type; };


} // namespace seec


#endif // SEEC_UTIL_FIXEDWIDTHINTTYPES_HPP
