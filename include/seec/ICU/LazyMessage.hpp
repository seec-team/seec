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

#include <memory>
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
  
  /// \brief Describe the message that would be loaded.
  ///
  virtual UnicodeString describe() const = 0;
};


/// \brief A LazyMessage that stores package and key details as pointers to
///        C strings.
///
class LazyMessageByRef : public LazyMessage {
  char const *Package;
  
  std::vector<char const *> Keys;
  
  std::vector<UnicodeString> ArgumentNames;
  
  std::vector<Formattable> ArgumentValues;
  
  /// \brief Constructor.
  ///
  LazyMessageByRef(char const *ThePackage,
                   std::vector<char const *> TheKeys,
                   std::vector<UnicodeString> TheArgumentNames,
                   std::vector<Formattable> TheArgumentValues)
  : Package(ThePackage),
    Keys(std::move(TheKeys)),
    ArgumentNames(std::move(TheArgumentNames)),
    ArgumentValues(std::move(TheArgumentValues))
  {}
  
  template<typename ...T>
  LazyMessageByRef(char const *ThePackage,
                   std::vector<char const *> TheKeys,
                   std::pair<char const *, T> &&...Arguments)
  : Package(ThePackage),
    Keys(std::move(TheKeys)),
    ArgumentNames{},
    ArgumentValues{}
  {}
  
protected:
  virtual UnicodeString create(UErrorCode &Status,
                               Locale const &GetLocale) const;
  
public:
  /// \brief Create a new LazyMessageByRef.
  ///
  template<typename ...T>
  static std::unique_ptr<LazyMessage>
  create(char const *ThePackage,
         std::vector<char const *> TheKeys,
         std::pair<char const *, T> &&...Arguments)
  {
    auto Message
      = new LazyMessageByRef(ThePackage,
                             std::move(TheKeys),
                             std::vector<UnicodeString>
                                  {UnicodeString::fromUTF8(Arguments.first)...},
                             std::vector<Formattable>
                                  {Formattable(Arguments.second)...});
    
    return std::unique_ptr<LazyMessage>(Message);
  }
  
  /// \brief Describe the message that would be loaded.
  ///
  virtual UnicodeString describe() const {
    UnicodeString Description;
    
    Description += "<Package=";
    Description += Package;
    
    if (Keys.size()) {
      Description += ", Keys=";
      Description += Keys[0];
      for (std::size_t i = 1; i < Keys.size(); ++i) {
        Description += "/";
        Description += Keys[i];
      }
    }
    
    if (ArgumentNames.size()) {
      Description += ", Arguments=";
      Description += "(";
      Description += ArgumentNames[0];
      Description += ")";
      
      for (std::size_t i = 1; i < ArgumentNames.size(); ++i) {
        Description += ",(";
        Description += ArgumentNames[i];
        Description += ")";
      }
    }
    
    Description += ">";
    
    return Description;
  }
};


} // namespace seec

#endif // SEEC_ICU_LAZYMESSAGE_HPP
