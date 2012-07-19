#include "seec/Preprocessor/MakeMemberFnChecker.hpp"
#include "seec/RuntimeErrors/ArgumentTypes.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Util/ConstExprMath.hpp"

#include "llvm/Support/ErrorHandling.h"

#include <type_traits>

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// RuntimeValueRecord
//------------------------------------------------------------------------------

llvm::raw_ostream & operator<<(llvm::raw_ostream &Out,
                               RuntimeValueRecord const &Record) {
  return Out << "<union>";
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
  seec::util::Maybe<uint64_t>>::type
getProcessTime(RecordT const &Record) {
  return Record.getProcessTime();
}

template<typename RecordT>
typename std::enable_if<
  !has_get_process_time<RecordT,
                        uint64_t const &(RecordT::*)() const>::value,
  seec::util::Maybe<uint64_t>>::type
getProcessTime(RecordT const &Record) {
  return seec::util::Maybe<uint64_t>();
}

seec::util::Maybe<uint64_t> EventRecordBase::getProcessTime() const {
  switch (getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return seec::trace::getProcessTime(                                      \
              *(static_cast<EventRecord<EventType::NAME> const *>(this)));
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return seec::util::Maybe<uint64_t>();
}


//------------------------------------------------------------------------------
// EventRecordBase::getThreadTime
//------------------------------------------------------------------------------

SEEC_PP_MAKE_MEMBER_FN_CHECKER(has_get_thread_time, getThreadTime)

template<typename RecordT>
typename std::enable_if<
  has_get_thread_time<RecordT,
                       uint64_t const &(RecordT::*)() const>::value,
  seec::util::Maybe<uint64_t>>::type
getThreadTime(RecordT const &Record) {
  return Record.getThreadTime();
}

template<typename RecordT>
typename std::enable_if<
  !has_get_thread_time<RecordT,
                        uint64_t const &(RecordT::*)() const>::value,
  seec::util::Maybe<uint64_t>>::type
getThreadTime(RecordT const &Record) {
  return seec::util::Maybe<uint64_t>();
}

seec::util::Maybe<uint64_t> EventRecordBase::getThreadTime() const {
  switch (getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return seec::trace::getThreadTime(                                       \
              *(static_cast<EventRecord<EventType::NAME> const *>(this)));
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return seec::util::Maybe<uint64_t>();
}


//------------------------------------------------------------------------------
// EventRecordBase::getIndex
//------------------------------------------------------------------------------

SEEC_PP_MAKE_MEMBER_FN_CHECKER(has_get_index, getIndex)

template<typename RecordT>
typename std::enable_if<
  has_get_index<RecordT, uint32_t const &(RecordT::*)() const>::value,
  seec::util::Maybe<uint32_t>>::type
getIndex(RecordT const &Record) {
  return Record.getIndex();
}

template<typename RecordT>
typename std::enable_if<
  !has_get_index<RecordT, uint32_t const &(RecordT::*)() const>::value,
  seec::util::Maybe<uint32_t>>::type
getIndex(RecordT const &Record) {
  return seec::util::Maybe<uint32_t>();
}

seec::util::Maybe<uint32_t> EventRecordBase::getIndex() const {
  switch (getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return seec::trace::getIndex(                                            \
              *(static_cast<EventRecord<EventType::NAME> const *>(this)));
#include "seec/Trace/Events.def"
    default: llvm_unreachable("Reference to unknown event type!");
  }
  
  return seec::util::Maybe<uint32_t>();
}


llvm::raw_ostream & operator<<(llvm::raw_ostream &Out,
                               EventRecordBase const &Event) {
  switch (Event.getType()) {
    case EventType::None:
      return Out << "<none>";

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    case EventType::NAME:                                                      \
      return Out << static_cast<EventRecord<EventType::NAME> const &>(Event);  \

#include "seec/Trace/Events.def"

    default:
      llvm_unreachable("Encountered unknown EventType.");
  }
}


//------------------------------------------------------------------------------
// Event records
//------------------------------------------------------------------------------

// Define raw_ostream output for all event records.
#define SEEC_PP_MEMBER(TYPE, NAME)                                             \
  if (Out.is_displayed()) {                                                    \
    Out.changeColor(llvm::raw_ostream::CYAN);                                  \
    Out << " " #NAME;                                                          \
    Out.changeColor(llvm::raw_ostream::BLUE);                                  \
    Out << "=";                                                                \
    Out.changeColor(llvm::raw_ostream::WHITE);                                 \
    Out << Event.get##NAME();                                                  \
    Out.resetColor();                                                          \
  }                                                                            \
  else {                                                                       \
    Out << " " #NAME "=" << Event.get##NAME();                                 \
  }                                                                            \

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,                          \
                              EventRecord<EventType::NAME> const &Event) {     \
  Out << "[";                                                                  \
  if (Out.is_displayed()) {                                                    \
    Out.changeColor(llvm::raw_ostream::GREEN);                                 \
    Out << #NAME;                                                              \
    Out.resetColor();                                                          \
  }                                                                            \
  else {                                                                       \
    Out << #NAME;                                                              \
  }                                                                            \
  SEEC_PP_APPLY(SEEC_PP_MEMBER, MEMBERS)                                       \
  Out << "]";                                                                  \
  return Out;                                                                  \
}

#include "seec/Trace/Events.def"

#undef SEEC_PP_MEMBER


#if 0
static_assert(std::is_trivially_copyable<EventRecordBase>::value,
              "EventRecordBase is not trivially copyable!");

// ensure that all event records are trivial
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
static_assert(std::is_trivially_copyable<EventRecord<EventType::NAME>>::value, \
              "EventRecord<" #NAME "> is not trivially copyable!");
#include "seec/Trace/Events.def"
#endif


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

} // namespace trace (in seec)

} // namespace seec
