//===- tools/seec-trace-view/ActionRecord.hpp -----------------------------===//
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

#ifndef SEEC_TRACE_VIEW_ACTIONRECORD_HPP
#define SEEC_TRACE_VIEW_ACTIONRECORD_HPP

#include "seec/Util/Parsing.hpp"

#include <wx/xml/xml.h>

#include <chrono>
#include <memory>
#include <vector>


namespace seec {
  namespace cm {
    class ProcessTrace;
  }
}

class wxPoint;
class wxSize;
class wxXmlDocument;
class wxXmlNode;



/// \brief Interface for an attribute.
///
class IAttributeReadOnly
{
protected:
  virtual std::string const &get_name_impl() const = 0;
  virtual std::string to_string_impl() const = 0;
  
public:
  virtual ~IAttributeReadOnly() =0;
  std::string const &get_name() const { return get_name_impl(); }
  std::string to_string() const { return to_string_impl(); }
};

/// \brief Interface for a writable attribute.
///
class IAttributeReadWrite : public IAttributeReadOnly
{
protected:
  virtual bool from_string_impl(std::string const &) = 0;
  
public:
  bool from_string(std::string const &S) { return from_string_impl(S); }
};

/// \brief A single attribute of a recorded event.
///
template<typename ValueT, typename Enable = void>
class Attribute; // undefined

/// \brief Check if a type is a non-writable (const or rvalue) arithmetic type.
///
template<typename T, typename DerefT = typename std::remove_reference<T>::type>
struct is_ro_arithmetic
: std::integral_constant<bool,
    std::is_arithmetic<DerefT>::value
    && (!std::is_lvalue_reference<T>::value || std::is_const<DerefT>::value)>
{};

/// \brief A single read-only attribute of a recorded event.
///
/// Generic implementation for arithmetic types using std::to_string().
///
template<typename T>
class Attribute<T, typename std::enable_if<is_ro_arithmetic<T>::value>::type>
: public IAttributeReadOnly
{
  std::string const Name;
  T const &Value;

protected:
  std::string const &get_name_impl() const { return Name; }
  std::string to_string_impl() const { return std::to_string(Value); }
  
public:
  Attribute(std::string WithName, T const &WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

/// \brief Check if a type is a writable (non-const lvalue) arithmetic type.
///
template<typename T, typename DerefT = typename std::remove_reference<T>::type>
struct is_rw_arithmetic
: std::integral_constant<bool,
    std::is_arithmetic<DerefT>::value
    && (std::is_lvalue_reference<T>::value && !std::is_const<DerefT>::value)>
{};

/// \brief A single attribute of a recorded event.
///
/// Generic implementation using std::to_string() and seec::parseTo().
///
template<typename T>
class Attribute<T, typename std::enable_if<is_rw_arithmetic<T>::value>::type>
: public IAttributeReadWrite
{
  std::string const Name;
  T &Value;

protected:
  std::string const &get_name_impl() const { return Name; }
  std::string to_string_impl() const { return std::to_string(Value); }
  bool from_string_impl(std::string const &String) {
    return seec::parseTo(String, Value);
  }
  
public:
  Attribute(std::string WithName, T &WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

/// \brief A single read-only attribute of a recorded event.
///
/// Implementation for const string values.
///
template<unsigned N>
class Attribute<char const (&)[N]>
: public IAttributeReadOnly
{
  std::string const Name;
  char const * const Value;

protected:
  std::string const &get_name_impl() const { return Name; }
  std::string to_string_impl() const { return Value; }
  
public:
  Attribute(std::string WithName, char const (&WithValue)[N])
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

/// \brief A single attribute of a recorded event.
///
/// Implementation for std::string values.
///
template<>
class Attribute<std::string &>
: public IAttributeReadWrite
{
  std::string const Name;
  std::string &Value;

protected:
  std::string const &get_name_impl() const { return Name; }
  std::string to_string_impl() const { return Value; }
  bool from_string_impl(std::string const &String) {
    Value = String;
    return true;
  }
  
public:
  Attribute(std::string WithName, std::string &WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

/// \brief Create an Attribute<T> with the given name and value.
///
template<typename T>
Attribute<T> make_attribute(std::string Name, T &&Value)
{
  return Attribute<T>(std::move(Name), std::forward<T>(Value));
}

/// \brief Records user interactions with the trace viewer.
///
class ActionRecord
{
  /// The process trace that the user is viewing.
  seec::cm::ProcessTrace const &Trace;
  
  /// Whether or not recording is enabled for this record.
  bool Enabled;
  
  /// The time at which the record was created.
  std::chrono::time_point<std::chrono::steady_clock> Started;
  
  /// Used to record user interactions.
  std::unique_ptr<wxXmlDocument> RecordDocument;
  
  /// The most recently inserted node in the record.
  wxXmlNode *LastNode;
  
public:
  /// \brief Create a new action record.
  ///
  ActionRecord(seec::cm::ProcessTrace const &ForTrace);
  
  /// \brief Enable recording for this record.
  ///
  void enable();
  
  /// \brief Disable recording for this record.
  ///
  void disable();
  
  /// \brief Record an event.
  ///
  void recordEventV(std::string const &Handler,
                    std::vector<IAttributeReadOnly const *> const &Attributes);
  
  /// \brief Record an event.
  ///
  template<typename... Ts>
  void recordEventL(std::string const &Handler,
                    Ts &&... Attributes)
  {
    if (Enabled)
      recordEventV(Handler, std::vector<IAttributeReadOnly const *>
                                       {&Attributes...});
  }
  
  /// \brief Finish this action record and submit it to the server.
  ///
  bool finalize();
};

#endif // SEEC_TRACE_VIEW_ACTIONRECORD_HPP
