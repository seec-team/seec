//===- lib/Trace/TraceFormat.cpp ------------------------------------------===//
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

#include "seec/Preprocessor/MakeMemberFnChecker.hpp"
#include "seec/RuntimeErrors/ArgumentTypes.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Util/ConstExprMath.hpp"

#include "llvm/Support/ErrorHandling.h"

#include <type_traits>

namespace seec {

namespace trace {


/// \brief Default printing for event objects and primitives.
class EventPrinterBase {
protected:
  /// The output stream we print to.
  llvm::raw_ostream &Out;
  
  /// \brief Construct an EventPrinterBase that prints to OutStream.
  /// \param OutStream the stream to print output to.
  EventPrinterBase(llvm::raw_ostream &OutStream)
  : Out(OutStream)
  {}
  
public:
  /// \brief Forward all objects with unhandled types to the output stream.
  /// \tparam T the type of the object.
  /// \param Value the object.
  template<typename T>
  EventPrinterBase &operator<<(T &&Value) {
    Out << std::forward<T>(Value);
    return *this;
  }
  
  /// \brief Print unsigned char values as unsigned integers.
  EventPrinterBase &operator<<(unsigned char Value) {
    Out << static_cast<unsigned int>(Value);
    return *this;
  }
  
  /// \brief Print signed char values as integers.
  EventPrinterBase &operator<<(signed char Value) {
    Out << static_cast<int>(Value);
    return *this;
  }

  /// \brief Check offsets against \c noOffset() when printing.
  EventPrinterBase &operator<<(offset_uint const Value) {
    if (Value == noOffset())
      Out << "<none>";
    else
      Out << Value;
    return *this;
  }
  
  /// \brief Wrap llvm::raw_ostream::changeColor().
  EventPrinterBase &changeColor(llvm::raw_ostream::Colors Color,
                                bool Bold = false,
                                bool BG = false) {
    Out.changeColor(Color, Bold, BG);
    return *this;
  }
  
  /// \brief Wrap llvm::raw_ostream::resetColor().
  EventPrinterBase &resetColor() {
    Out.resetColor();
    return *this;
  }
  
  /// \brief Wrap llvm::raw_ostream::is_displayed().
  bool is_displayed() const { return Out.is_displayed(); }
  
  /// \brief Default printing scheme for members.
  template<typename T>
  EventPrinterBase &printMember(llvm::StringRef Name, T &&Value) {
    if (is_displayed()) {
      changeColor(llvm::raw_ostream::CYAN);
      *this << " " << Name;
      changeColor(llvm::raw_ostream::BLUE);
      *this << "=";
      changeColor(llvm::raw_ostream::WHITE);
      *this << Value;
      resetColor();
    }
    else {
      *this << " " << Name << "=" << Value;
    }
    
    return *this;
  }
};


/// \brief Used to print event objects.
class EventPrinter : public EventPrinterBase {
public:
  EventPrinter(llvm::raw_ostream &OutStream)
  : EventPrinterBase(OutStream)
  {}
};


/// \brief Implements default printing for event members.
template<EventType ET>
class MemberFormatterBase; // Will be specialized by X-Macro.


/// \brief Handle printing for event members.
/// This template class simply inherits the default implementations from
/// MemberFormatterBase. Specialize the template to implement custom member
/// printing for an event type.
template<EventType ET>
class MemberFormatter : public MemberFormatterBase<ET> {
public:
  MemberFormatter(EventPrinter &Out)
  : MemberFormatterBase<ET>(Out)
  {}
};


/// \brief Implements default printing for events.
template<EventType ET>
class EventFormatterBase; // Will be specialized by X-Macro.


/// \brief Handle printing for events.
/// This template class simply inherits the default implementation from
/// EventFormatterBase. Specialize the template to implement custom printing
/// for an event type.
template<EventType ET>
class EventFormatter : public EventFormatterBase<ET> {
public:
  EventFormatter(EventPrinter &Out)
  : EventFormatterBase<ET>(Out)
  {}
};


char const *describe(EventType Type) {
  switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
    case EventType::NAME: return #NAME;
#include "seec/Trace/Events.def"
    default:
      return "Unknown EventType";
  }
}


//------------------------------------------------------------------------------
// EventRecordBase
//------------------------------------------------------------------------------

std::size_t EventRecordBase::getEventSize() const {
  switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
    case EventType::NAME: return sizeof(EventRecord<EventType::NAME>);
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return 0;
}


//------------------------------------------------------------------------------
// EventRecordBase::getProcessTime
//------------------------------------------------------------------------------

SEEC_PP_MAKE_MEMBER_FN_CHECKER(has_get_process_time, getProcessTime)

template<typename RecordT>
typename std::enable_if<
  has_get_process_time<RecordT,
                       uint64_t const &(RecordT::*)() const>::value,
  seec::Maybe<uint64_t>>::type
getProcessTime(RecordT const &Record) {
  return Record.getProcessTime();
}

template<typename RecordT>
typename std::enable_if<
  !has_get_process_time<RecordT,
                        uint64_t const &(RecordT::*)() const>::value,
  seec::Maybe<uint64_t>>::type
getProcessTime(RecordT const &Record) {
  return seec::Maybe<uint64_t>();
}

seec::Maybe<uint64_t> EventRecordBase::getProcessTime() const {
  switch (getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return seec::trace::getProcessTime(                                      \
              *(static_cast<EventRecord<EventType::NAME> const *>(this)));
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return seec::Maybe<uint64_t>();
}


//------------------------------------------------------------------------------
// EventRecordBase::getThreadTime
//------------------------------------------------------------------------------

SEEC_PP_MAKE_MEMBER_FN_CHECKER(has_get_thread_time, getThreadTime)

template<typename RecordT>
typename std::enable_if<
  has_get_thread_time<RecordT,
                       uint64_t const &(RecordT::*)() const>::value,
  seec::Maybe<uint64_t>>::type
getThreadTime(RecordT const &Record) {
  return Record.getThreadTime();
}

template<typename RecordT>
typename std::enable_if<
  !has_get_thread_time<RecordT,
                        uint64_t const &(RecordT::*)() const>::value,
  seec::Maybe<uint64_t>>::type
getThreadTime(RecordT const &Record) {
  return seec::Maybe<uint64_t>();
}

seec::Maybe<uint64_t> EventRecordBase::getThreadTime() const {
  switch (getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return seec::trace::getThreadTime(                                       \
              *(static_cast<EventRecord<EventType::NAME> const *>(this)));
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return seec::Maybe<uint64_t>();
}


//------------------------------------------------------------------------------
// EventRecordBase::getIndex
//------------------------------------------------------------------------------

SEEC_PP_MAKE_MEMBER_FN_CHECKER(has_get_index, getIndex)

template<typename RecordT>
typename std::enable_if<
  has_get_index<RecordT, uint32_t const &(RecordT::*)() const>::value,
  seec::Maybe<uint32_t>>::type
getIndex(RecordT const &Record) {
  return Record.getIndex();
}

template<typename RecordT>
typename std::enable_if<
  !has_get_index<RecordT, uint32_t const &(RecordT::*)() const>::value,
  seec::Maybe<uint32_t>>::type
getIndex(RecordT const &Record) {
  return seec::Maybe<uint32_t>();
}

seec::Maybe<uint32_t> EventRecordBase::getIndex() const {
  switch (getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return seec::trace::getIndex(                                            \
              *(static_cast<EventRecord<EventType::NAME> const *>(this)));
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return seec::Maybe<uint32_t>();
}


//------------------------------------------------------------------------------
// Default printing for event members.
//------------------------------------------------------------------------------

// Specialize MemberFormatterBase for each event type.
#define SEEC_PP_MEMBER_PRINTER(TYPE, NAME)                                     \
  void print##NAME(EventRecordTy const &Event) {                               \
    Out.printMember(#NAME, Event.get##NAME());                                 \
  }

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
template<>                                                                     \
class MemberFormatterBase<EventType::NAME> {                                   \
protected:                                                                     \
  typedef EventRecord<EventType::NAME> EventRecordTy;                          \
  EventPrinter &Out;                                                           \
public:                                                                        \
  MemberFormatterBase(EventPrinter &Out) : Out(Out) {}                         \
  SEEC_PP_APPLY(SEEC_PP_MEMBER_PRINTER, MEMBERS)                               \
};

#include "seec/Trace/Events.def"

#undef SEEC_PP_MEMBER_PRINTER


//------------------------------------------------------------------------------
// Specialized printing for event members.
//------------------------------------------------------------------------------

template<>
class MemberFormatter<EventType::RuntimeError>
: public MemberFormatterBase<EventType::RuntimeError> {
public:
  MemberFormatter(EventPrinter &Out)
  : MemberFormatterBase<EventType::RuntimeError>(Out)
  {}
  
  void printErrorType(EventRecord<EventType::RuntimeError> const &Event) {
    Out.printMember("ErrorType",
                    describe(static_cast<seec::runtime_errors::RunErrorType>
                                        (Event.getErrorType())));
  }
};


template<>
class MemberFormatter<EventType::RuntimeErrorArgument>
: public MemberFormatterBase<EventType::RuntimeErrorArgument> {
public:
  MemberFormatter(EventPrinter &Out)
  : MemberFormatterBase<EventType::RuntimeErrorArgument>(Out)
  {}
  
  void printArgumentType(
          EventRecord<EventType::RuntimeErrorArgument> const &Event) {
    Out.printMember("ArgumentType",
                    describe(static_cast<seec::runtime_errors::ArgType>
                                        (Event.getArgumentType())));
  }
};


//------------------------------------------------------------------------------
// Default printing for events.
//------------------------------------------------------------------------------

// Specialize EventFormatterBase for all event types.
#define SEEC_PP_CALL_MEMBER_PRINTER(TYPE, NAME)                                \
  MemberPrinter.print##NAME(Event);

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
template<>                                                                     \
class EventFormatterBase<EventType::NAME> {                                    \
  EventPrinter &Out;                                                           \
public:                                                                        \
  EventFormatterBase(EventPrinter &Out) : Out(Out) {}                          \
  void print(EventRecord<EventType::NAME> const &Event) {                      \
    Out << "[";                                                                \
    if (Out.is_displayed()) {                                                  \
      Out.changeColor(llvm::raw_ostream::GREEN);                               \
      Out << #NAME;                                                            \
      Out.resetColor();                                                        \
    }                                                                          \
    else {                                                                     \
      Out << #NAME;                                                            \
    }                                                                          \
    MemberFormatter<EventType::NAME> MemberPrinter(Out);                       \
    SEEC_PP_APPLY(SEEC_PP_CALL_MEMBER_PRINTER, MEMBERS)                        \
    Out << "]";                                                                \
  }                                                                            \
};

#include "seec/Trace/Events.def"

#undef SEEC_PP_CALL_MEMBER_PRINTER

//------------------------------------------------------------------------------
// Specialized printing for events.
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Check that all event records meet the requirements (not active).
//------------------------------------------------------------------------------

#if 0

static_assert(std::is_trivially_copyable<EventRecordBase>::value,
              "EventRecordBase is not trivially copyable!");

// ensure that all event records are trivial
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
static_assert(std::is_trivially_copyable<EventRecord<EventType::NAME>>::value, \
              "EventRecord<" #NAME "> is not trivially copyable!");
#include "seec/Trace/Events.def"

// check the alignment requirements of event records
constexpr std::size_t MaximumRecordAlignment() {
  return seec::const_math::max(
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
                               alignof(EventRecord<EventType::NAME>),
#include "seec/Trace/Events.def"
                               0u);
}

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
static_assert(sizeof(EventRecord<EventType::NAME>) >= MaximumRecordAlignment(),\
              "EventRecord<" #NAME "> size is below MaximumRecordAlignment()");\
static_assert(                                                                 \
  sizeof(EventRecord<EventType::NAME>) % MaximumRecordAlignment() == 0,        \
  "EventRecord<" #NAME "> size not divisible by MaximumRecordAlignment()");
#include "seec/Trace/Events.def"

#endif // 0

//------------------------------------------------------------------------------
// EventRecord output for llvm::raw_ostream
//------------------------------------------------------------------------------

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,                          \
                              EventRecord<EventType::NAME> const &Event) {     \
  EventPrinter Printer(Out);                                                   \
  EventFormatter<EventType::NAME> Formatter(Printer);                          \
  Formatter.print(Event);                                                      \
  return Out;                                                                  \
}
#include "seec/Trace/Events.def"

//------------------------------------------------------------------------------
// EventRecordBase output for llvm::raw_ostream
//------------------------------------------------------------------------------

llvm::raw_ostream &operator<<(llvm::raw_ostream &OutStream,
                              EventRecordBase const &Event) {
  switch (Event.getType()) {
    case EventType::None:
      return OutStream << "<none>";

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return OutStream                                                         \
             << static_cast<EventRecord<EventType::NAME> const &>(Event);
#include "seec/Trace/Events.def"

    default:
      llvm_unreachable("Encountered unknown EventType.");
  }
}


} // namespace trace (in seec)

} // namespace seec
