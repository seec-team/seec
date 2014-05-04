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

#include <wx/panel.h>
#include <wx/xml/xml.h>

#include <chrono>
#include <memory>
#include <vector>


namespace clang {
  class Decl;
  class Stmt;
}

namespace seec {
  namespace cm {
    class ProcessTrace;
  }
}

class wxButton;
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
  virtual std::string to_string_impl(seec::cm::ProcessTrace const &) const = 0;
  
public:
  virtual ~IAttributeReadOnly() =0;
  std::string const &get_name() const { return get_name_impl(); }
  std::string to_string(seec::cm::ProcessTrace const &Trace) const {
    return to_string_impl(Trace);
  }
};

/// \brief Interface for a writable attribute.
///
class IAttributeReadWrite : public IAttributeReadOnly
{
protected:
  virtual bool from_string_impl(seec::cm::ProcessTrace const &,
                                std::string const &) = 0;

public:
  bool from_string(seec::cm::ProcessTrace const &Trace,
                   std::string const &S)
  {
    return from_string_impl(Trace, S);
  }
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
  T const Value;

protected:
  std::string const &get_name_impl() const { return Name; }

  std::string to_string_impl(seec::cm::ProcessTrace const &) const {
    return std::to_string(Value);
  }
  
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

  std::string to_string_impl(seec::cm::ProcessTrace const &) const {
    return std::to_string(Value);
  }

  bool from_string_impl(seec::cm::ProcessTrace const &,
                        std::string const &String)
  {
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
template<>
class Attribute<std::string>
: public IAttributeReadOnly
{
  std::string const Name;
  std::string const Value;

protected:
  std::string const &get_name_impl() const { return Name; }

  std::string to_string_impl(seec::cm::ProcessTrace const &) const {
    return Value;
  }

public:
  Attribute(std::string WithName, std::string WithValue)
  : Name(std::move(WithName)),
    Value(std::move(WithValue))
  {}
};

/// \brief A single read-only attribute of a recorded event.
///
/// Implementation for const string values.
///
template<>
class Attribute<std::string const &>
: public Attribute<std::string>
{
public:
  Attribute(std::string WithName, std::string const &WithValue)
  : Attribute<std::string>(std::move(WithName), WithValue)
  {}
};

/// \brief A single read-only attribute of a recorded event.
///
/// Implementation for const string values.
///
template<unsigned N>
class Attribute<char const (&)[N]>
: public Attribute<std::string>
{
public:
  Attribute(std::string WithName, char const (&WithValue)[N])
  : Attribute<std::string>(std::move(WithName), WithValue)
  {}
};

/// \brief A single read-only attribute of a recorded event.
///
/// Implementation for const string values.
///
template<>
class Attribute<char const *>
: public Attribute<std::string>
{
public:
  Attribute(std::string WithName, char const *WithValue)
  : Attribute<std::string>(std::move(WithName), WithValue)
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

  std::string to_string_impl(seec::cm::ProcessTrace const &) const {
    return Value;
  }

  bool from_string_impl(seec::cm::ProcessTrace const &,
                        std::string const &String)
  {
    Value = String;
    return true;
  }
  
public:
  Attribute(std::string WithName, std::string &WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

/// \brief A single attribute of a recorded event.
///
/// Implementation for clang::Decl const * values.
///
class AttributeDeclReadOnlyBase : public IAttributeReadOnly
{
  std::string const Name;
  clang::Decl const * const Value;

protected:
  std::string const &get_name_impl() const { return Name; }

  std::string to_string_impl(seec::cm::ProcessTrace const &Trace) const;

public:
  AttributeDeclReadOnlyBase(std::string WithName,
                            clang::Decl const * const WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

class AttributeDeclReadWriteBase : public IAttributeReadWrite
{
  std::string const Name;
  clang::Decl const *&Value;

protected:
  std::string const &get_name_impl() const { return Name; }

  std::string to_string_impl(seec::cm::ProcessTrace const &Trace) const;

  bool from_string_impl(seec::cm::ProcessTrace const &Trace,
                        std::string const &String);

public:
  AttributeDeclReadWriteBase(std::string WithName,
                             clang::Decl const *&WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

template<>
class Attribute<clang::Decl const *&>
: public AttributeDeclReadWriteBase
{
public:
  Attribute(std::string WithName, clang::Decl const *&WithValue)
  : AttributeDeclReadWriteBase(std::move(WithName), WithValue)
  {}
};

template<>
class Attribute<clang::Decl const * const &>
: public AttributeDeclReadOnlyBase
{
public:
  Attribute(std::string WithName, clang::Decl const * const &WithValue)
  : AttributeDeclReadOnlyBase(std::move(WithName), WithValue)
  {}
};

/// \brief A single attribute of a recorded event.
///
/// Implementation for clang::Stmt const * values.
///
class AttributeStmtReadOnlyBase : public IAttributeReadOnly
{
  std::string const Name;
  clang::Stmt const * const Value;

protected:
  std::string const &get_name_impl() const { return Name; }

  std::string to_string_impl(seec::cm::ProcessTrace const &Trace) const;

public:
  AttributeStmtReadOnlyBase(std::string WithName,
                            clang::Stmt const * const WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

class AttributeStmtReadWriteBase : public IAttributeReadWrite
{
  std::string const Name;
  clang::Stmt const *&Value;

protected:
  std::string const &get_name_impl() const { return Name; }

  std::string to_string_impl(seec::cm::ProcessTrace const &Trace) const;

  bool from_string_impl(seec::cm::ProcessTrace const &Trace,
                        std::string const &String);

public:
  AttributeStmtReadWriteBase(std::string WithName, clang::Stmt const *&WithValue)
  : Name(std::move(WithName)),
    Value(WithValue)
  {}
};

template<>
class Attribute<clang::Stmt const *&>
: public AttributeStmtReadWriteBase
{
public:
  Attribute(std::string WithName, clang::Stmt const *&WithValue)
  : AttributeStmtReadWriteBase(std::move(WithName), WithValue)
  {}
};

template<>
class Attribute<clang::Stmt const * const &>
: public AttributeStmtReadOnlyBase
{
public:
  Attribute(std::string WithName, clang::Stmt const * const &WithValue)
  : AttributeStmtReadOnlyBase(std::move(WithName), WithValue)
  {}
};

/// \brief Create an Attribute<T> with the given name and value.
///
template<typename T>
Attribute<T> make_attribute(std::string Name, T &&Value)
{
  return Attribute<T>(std::move(Name), std::forward<T>(Value));
}

/// \brief Create a dynamically allocated \c Attribute<T> with the given name
///        and value.
///
template<typename T>
std::unique_ptr<Attribute<T>> new_attribute(std::string Name, T &&Value)
{
  return std::unique_ptr<Attribute<T>>
                        (new Attribute<T>(std::move(Name),
                                          std::forward<T>(Value)));
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
  
  /// \brief Write an archive of this recording (and trace) to the given stream.
  ///
  bool archiveTo(wxOutputStream &Stream);
  
public:
  /// \brief Create a new action record.
  ///
  ActionRecord(seec::cm::ProcessTrace const &ForTrace);
  
  /// \brief Check if the recording is enabled.
  ///
  bool isEnabled() const { return Enabled; }
  
  /// \brief Enable recording for this record.
  ///
  bool enable();
  
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

/// \brief A control that allows the user to enable/disable recording.
///
class ActionRecordingControl : public wxPanel
{
  ActionRecord *Recording;
  
  wxButton *ButtonEnable;
  
  wxImage ImgRecordingOn;
  
  wxImage ImgRecordingOff;
  
public:
  /// \brief Constructor (without creation).
  ///
  ActionRecordingControl()
  : Recording(nullptr),
    ButtonEnable(nullptr),
    ImgRecordingOn(),
    ImgRecordingOff()
  {}
  
  /// \brief Constructor (with creation).
  ///
  ActionRecordingControl(wxWindow *Parent, ActionRecord &WithRecord)
  : ActionRecordingControl()
  {
    Create(Parent, WithRecord);
  }
  
  /// \brief Create this object.
  ///
  bool Create(wxWindow *Parent, ActionRecord &WithRecord);
};

#endif // SEEC_TRACE_VIEW_ACTIONRECORD_HPP
