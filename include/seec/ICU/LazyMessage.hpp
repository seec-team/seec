//===- include/seec/ICU/LazyMessage.hpp ----------------------------- C++ -===//
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

#ifndef SEEC_ICU_LAZYMESSAGE_HPP
#define SEEC_ICU_LAZYMESSAGE_HPP


#include "unicode/fmtable.h"
#include "unicode/unistr.h"

#include <string>
#include <vector>


namespace seec {


/// \brief An ICU string that will be loaded and formatted lazily.
///
class LazyMessage {
  /// The final formatted message.
  UnicodeString Message;
  
  /// True iff we have attempted to create the message.
  bool Created;

protected:
  /// \brief Default constructor.
  ///
  LazyMessage()
  : Message(),
    Created(false)
  {}
  
  /// \brief Create the final formatted (implemented by subclasses).
  ///
  virtual UnicodeString create(UErrorCode &Status,
                               Locale const &GetLocale) const = 0;

public:
  /// \brief Get the final formatted string.
  ///
  UnicodeString const &get(UErrorCode &Status, Locale const &GetLocale) {
    if (!Created) {
      Message = create(Status, GetLocale);
      Created = true;
    }
    
    return Message;
  }
};


/// \brief A LazyMessage that stores package and key details as pointers to
///        C strings.
///
class LazyMessageByRef : public LazyMessage {
  char const *Package;
  
  std::vector<char const *> Keys;
  
  std::vector<UnicodeString> ArgumentNames;
  
  std::vector<Formattable> ArgumentValues;
  
protected:
  virtual UnicodeString create(UErrorCode &Status,
                               Locale const &GetLocale) const;
  
public:
  /// \brief Constructor.
  ///
  template<typename ...T>
  LazyMessageByRef(char const *ThePackage,
                   std::vector<char const *> TheKeys,
                   std::pair<char const *, T> &&...Arguments)
  : Package(ThePackage),
    Keys(std::move(TheKeys)),
    ArgumentNames{UnicodeString::fromUTF8(Arguments.first)...},
    ArgumentValues{Formattable(Arguments.second)...}
  {}
};


} // namespace seec

#endif // SEEC_ICU_LAZYMESSAGE_HPP
