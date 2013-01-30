//===- include/seec/ICU/Format.hpp ---------------------------------- C++ -===//
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

#ifndef SEEC_ICU_FORMAT_HPP
#define SEEC_ICU_FORMAT_HPP


#include "unicode/fmtable.h"
#include "unicode/msgfmt.h"
#include "unicode/unistr.h"

#include <cassert>
#include <vector>


namespace seec {

namespace icu {


/// \brief Holds a list of named format arguments.
///
class FormatArgumentsWithNames {
  /// Argument names.
  std::vector<UnicodeString> Names;
  
  /// Argument values.
  std::vector<Formattable> Values;
  
public:
  /// \name Constructors.
  /// @{
  
  FormatArgumentsWithNames()
  : Names(),
    Values()
  {}
  
  FormatArgumentsWithNames(FormatArgumentsWithNames const &) = default;
  
  FormatArgumentsWithNames(FormatArgumentsWithNames &&) = default;
  
  /// @} (Constructors)
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the number of arguments.
  std::size_t size() const { return Names.size(); }
  
  /// \brief Get a pointer to the array of argument names.
  UnicodeString const *names() const { return Names.data(); }
  
  /// \brief Get a pointer to the array of argument values.
  Formattable const *values() const { return Values.data(); }
  
  /// @} (Accessors)
  
  
  /// \name Mutators.
  /// @{
  
  FormatArgumentsWithNames &add(UnicodeString Name, Formattable Value) {
    Names.emplace_back(std::move(Name));
    Values.emplace_back(std::move(Value));
    assert(Names.size() == Values.size());
    return *this;
  }
  
  template<typename NameT, typename ValueT>
  FormatArgumentsWithNames &add(NameT &&Name, ValueT &&Value) {
    Names.emplace_back(std::forward<NameT>(Name));
    Values.emplace_back(std::forward<ValueT>(Value));
    assert(Names.size() == Values.size());
    return *this;
  }
  
  /// @} (Mutators)
};


UnicodeString format(UnicodeString const &FormatString,
                     FormatArgumentsWithNames const &Arguments,
                     UErrorCode &Status);


} // namespace icu (in seec)


template<typename... Ts>
UnicodeString format(UnicodeString FormatString,
                     UErrorCode &Status,
                     Ts... Arguments)
{
  UnicodeString Result;

  Formattable FmtArguments[] = {
    Formattable(Arguments)...
  };

  Result = MessageFormat::format(FormatString,
                                 FmtArguments,
                                 sizeof...(Ts),
                                 Result,
                                 Status);

  return Result;
}


} // namespace seec


#endif // SEEC_ICU_FORMAT_HPP
