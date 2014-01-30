//===- include/seec/Util/Parsing.hpp -------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_PARSING_HPP
#define SEEC_UTIL_PARSING_HPP

#include <cerrno>
#include <limits>
#include <string>

namespace seec {

template<typename>
struct ParseToImpl;

template<> struct ParseToImpl<int> {
  static bool impl(std::string const &In, int &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtol(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    if (Value < std::numeric_limits<int>::min()
        || Value > std::numeric_limits<int>::max())
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<long> {
  static bool impl(std::string const &In, long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtol(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<long long> {
  static bool impl(std::string const &In, long long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoll(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<unsigned> {
  static bool impl(std::string const &In, unsigned &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoul(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    if (Value > std::numeric_limits<unsigned>::max())
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<unsigned long> {
  static bool impl(std::string const &In, unsigned long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoul(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<unsigned long long> {
  static bool impl(std::string const &In, unsigned long long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoull(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<float> {
  static bool impl(std::string const &In, float &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtof(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<double> {
  static bool impl(std::string const &In, double &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtod(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<> struct ParseToImpl<long double> {
  static bool impl(std::string const &In, long double &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtold(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    return true;
  }
};

template<typename T>
bool parseTo(std::string const &In, T &Out) noexcept {
  return ParseToImpl<T>::impl(In, Out);
}

} // namespace seec

#endif // SEEC_UTIL_PARSING_HPP
