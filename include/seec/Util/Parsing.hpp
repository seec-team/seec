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
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   int &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
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
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<long> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   long &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtol(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<long long> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   long long &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoll(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<unsigned> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   unsigned &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
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
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<unsigned long> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   unsigned long &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoul(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<unsigned long long> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   unsigned long long &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtoull(Start, &End, 0);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<float> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   float &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtof(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<double> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   double &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtod(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

template<> struct ParseToImpl<long double> {
  static bool impl(std::string const &In,
                   std::string::size_type const StartChar,
                   long double &Out,
                   std::size_t &OutRead) noexcept
  {
    auto const Start = In.c_str() + StartChar;
    char *End = nullptr;
    
    errno = 0;
    auto const Value = std::strtold(Start, &End);
    if (errno)
      return false;
    
    // Check whether at least one character was converted.
    if (Value == 0 && (Start == End || *(End - 1) != '0'))
      return false;
    
    Out = Value;
    OutRead = static_cast<std::size_t>(End - Start);
    return true;
  }
};

/// \brief Attempt to parse a value of type \c T from \c In.
/// \tparam T the type of value to parse.
/// \param In the string to read from.
/// \param Start the character to start reading from.
/// \param Out object to store the parsed value in (if successful).
/// \param OutRead stores the number of characters consumed (if successful).
/// \return true iff the value was parsed successfully.
///
template<typename T>
bool parseTo(std::string const &In,
             std::string::size_type const StartChar,
             T &Out,
             std::size_t &OutRead)
noexcept
{
  if (StartChar >= In.size())
    return false;

  return ParseToImpl<T>::impl(In, StartChar, Out, OutRead);
}

template<typename T>
bool parseTo(std::string const &In, T &Out) noexcept {
  std::size_t DummyOutRead;
  return parseTo(In, 0, Out, DummyOutRead);
}

} // namespace seec

#endif // SEEC_UTIL_PARSING_HPP
