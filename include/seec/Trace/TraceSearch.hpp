//===- include/seec/Trace/TraceSearch.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_TRACESEARCH_HPP
#define SEEC_TRACE_TRACESEARCH_HPP

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/ArrayRef.h"

namespace seec {

namespace trace {

/// Base case of typeInList recursion - return false.
template<typename...>
bool typeInList(EventType Type) {
  return false;
}

/// Return true if an EventType is in a static list of values.
/// \tparam Head the first value in the list.
/// \tparam Tail the remaining values in the list.
/// \param Type the EventType to check.
/// \return true iff Type matches Head or one of the types in Tail.
template<EventType Head, EventType... Tail>
bool typeInList(EventType Type) {
  if (Head == Type)
    return true;
  return typeInList<Tail...>(Type);
}

/// Find the first Event in a range that matches a set of EventTypes.
/// \tparam SearchFor the EventTypes that will be accepted.
/// \param Range the range of Events to search over.
/// \return a pointer to the first Event in Range with type in SearchFor, or
///         nullptr if no such Event exists.
template<EventType... SearchFor>
seec::util::Maybe<EventReference> find(EventRange Range) {
  for (auto &&Ev : Range) {
    if (typeInList<SearchFor...>(Ev.getType())) {
      return seec::util::Maybe<EventReference>(EventReference(Ev));
    }
  }
  
  return seec::util::Maybe<EventReference>();
}

/// Find the last Event in a range that matches a set of EventTypes.
/// \tparam SearchFor the EventTypes that will be accepted.
/// \param Range the range of Events to search over.
/// \return a pointer to the last Event in Range with type in SearchFor, or
///         nullptr if no such Event exists.
template<EventType... SearchFor>
seec::util::Maybe<EventReference> rfind(EventRange Range) {
  auto PreBegin = --(Range.begin());
  
  for (auto It = --(Range.end()); It != PreBegin; --It) {
    if (typeInList<SearchFor...>(It->getType())) {
      return seec::util::Maybe<EventReference>(It);
    }
  }
  
  return seec::util::Maybe<EventReference>();
}

/// Find the first Event in a range for which a predicate returns true.
/// \tparam PredT the type of the predicate.
/// \param Range the event range to search in.
/// \param Predicate the predicate to check the events with.
/// \return a seec::util::Maybe, which contains an EventReference for the first
///         Event in Range for which Predicate(Event) is true. If no such event
///         is found, the Maybe is unassigned.
template<typename PredT>
seec::util::Maybe<EventReference> find(EventRange Range, PredT Predicate) {
  for (auto &&Ev : Range) {
    if (Predicate(Ev)) {
      return seec::util::Maybe<EventReference>(EventReference(Ev));
    }
  }
  
  return seec::util::Maybe<EventReference>();
}

/// Find the last Event in a range for which a predicate returns true.
/// \tparam PredT the type of the predicate.
/// \param Range the event range to search in.
/// \param Predicate the predicate to check the events with.
/// \return a seec::util::Maybe, which contains an EventReference for the last
///         Event in Range for which Predicate(Event) is true. If no such event
///         is found, the Maybe is unassigned.
template<typename PredT>
seec::util::Maybe<EventReference> rfind(EventRange Range, PredT Predicate) {
  auto PreBegin = --(Range.begin());
  
  for (auto It = --(Range.end()); It != PreBegin; --It) {
    if (Predicate(*It)) {
      return seec::util::Maybe<EventReference>(It);
    }
  }
  
  return seec::util::Maybe<EventReference>();
}

/// Get the events in an EventRange prior to a specific event.
/// \param Range the range of events.
/// \param Ev a specific event in Range.
/// \return an EventRange containing the events in Range prior to Ev.
inline EventRange rangeBefore(EventRange Range, EventReference Ev) {
  assert(Range.begin() <= Ev && Ev <= Range.end());
  
  return EventRange(Range.begin(), Ev);
}

/// Get the events in an EventRange prior to and including a specific event.
/// \param Range the range of events.
/// \param Ev a specific event in Range.
/// \return an EventRange containing the events in Range prior to and including
///         Ev.
inline EventRange rangeBeforeIncluding(EventRange Range, EventReference Ev) {
  assert(Range.begin() <= Ev && Ev <= Range.end());
  
  if (Ev != Range.end())
    return EventRange(Range.begin(), ++Ev);
  
  return EventRange(Range.begin(), Ev);
}

/// Get the events in an EventRange following a specific event.
/// \param Range the range of events.
/// \param Ev a specific event in Range.
/// \return an EventRange containing the events in Range following Ev.
inline EventRange rangeAfter(EventRange Range, EventReference Ev) {
  assert(Range.begin() <= Ev && Ev <= Range.end());
  
  if (Ev != Range.end())
    return EventRange(++Ev, Range.end());
  
  // range does not exist, so return an empty range.
  return EventRange(Ev, Ev);
}

/// Get the events in an EventRange following and including a specific event.
/// \param Range the range of events.
/// \param Ev a specific event in Range.
/// \return an EventRange containing the events in Range following and including
///         Ev.
inline EventRange rangeAfterIncluding(EventRange Range, EventReference Ev) {
  assert(Range.begin() <= Ev && Ev <= Range.end());
  
  return EventRange(Ev, Range.end());
}

/// Get an ArrayRef of all the consecutive event records of type ET at the
/// start of the given event range.
/// \tparam ET the type of events in the block that is being found.
/// \param Range a range of events that starts with the block.
/// \return a reference to all the consecutive event records of type ET at the
///         start of Range.
template<EventType ET>
llvm::ArrayRef<EventRecord<ET>> getLeadingBlock(EventRange Range) {
  std::size_t Count = 0;
  
  for (auto EvRef = Range.begin(); EvRef != Range.end(); ++EvRef) {
    if (EvRef->getType() != ET)
      break;
    ++Count;
  }
  
  if (!Count)
    return llvm::ArrayRef<EventRecord<ET>>();
  
  return llvm::ArrayRef<EventRecord<ET>>(&(Range.begin().get<ET>()), Count);
}

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESEARCH_HPP
