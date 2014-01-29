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
#include <string>

namespace seec {

template<typename>
struct ParseToImpl;

template<> struct ParseToImpl<long> {
  static bool impl(std::string const &In, long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtol(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<> struct ParseToImpl<long long> {
  static bool impl(std::string const &In, long long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtoll(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<> struct ParseToImpl<unsigned long> {
  static bool impl(std::string const &In, unsigned long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtoul(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<> struct ParseToImpl<unsigned long long> {
  static bool impl(std::string const &In, unsigned long long &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtoull(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<> struct ParseToImpl<float> {
  static bool impl(std::string const &In, float &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtof(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<> struct ParseToImpl<double> {
  static bool impl(std::string const &In, double &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtod(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<> struct ParseToImpl<long double> {
  static bool impl(std::string const &In, long double &Out) noexcept {
    auto const Start = In.c_str();
    char *End = nullptr;
    
    errno = 0;
    Out = std::strtold(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Out == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    return true;
  }
};

template<typename T>
bool parseTo(std::string const &In, T &Out) noexcept {
  return ParseToImpl<T>::impl(In, Out);
}

} // namespace seec

#endif // SEEC_UTIL_PARSING_HPP
