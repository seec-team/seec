//===- include/seec/Trace/TraceSearch.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACESEARCH_HPP
#define SEEC_TRACE_TRACESEARCH_HPP

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/FunctionTraits.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/ArrayRef.h"

namespace seec {

namespace trace {


/// \name Search by EventType
/// @{

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
seec::Maybe<EventReference> find(EventRange Range) {
  for (auto &&Ev : Range) {
    if (typeInList<SearchFor...>(Ev.getType())) {
      return seec::Maybe<EventReference>(Ev);
    }
  }

  return seec::Maybe<EventReference>();
}

/// Find the last Event in a range that matches a set of EventTypes.
/// \tparam SearchFor the EventTypes that will be accepted.
/// \param Range the range of Events to search over.
/// \return a pointer to the last Event in Range with type in SearchFor, or
///         nullptr if no such Event exists.
template<EventType... SearchFor>
seec::Maybe<EventReference> rfind(EventRange Range) {
  if (Range.begin() == Range.end())
    return seec::Maybe<EventReference>();
  
  for (auto It = --(Range.end()); ; --It) {
    if (typeInList<SearchFor...>(It->getType())) {
      return seec::Maybe<EventReference>(It);
    }
    
    if (It == Range.begin())
      break;
  }

  return seec::Maybe<EventReference>();
}

/// @}


/// \name Search by Predicate
/// @{

/// Find the first Event in a range for which a predicate returns true.
/// \tparam PredT the type of the predicate.
/// \param Range the event range to search in.
/// \param Predicate the predicate to check the events with.
/// \return a seec::Maybe, which contains an EventReference for the first
///         Event in Range for which Predicate(Event) is true. If no such event
///         is found, the Maybe is unassigned.
template<typename PredT>
seec::Maybe<EventReference> find(EventRange Range, PredT Predicate) {
  for (auto &&Ev : Range) {
    if (Predicate(Ev)) {
      return seec::Maybe<EventReference>(Ev);
    }
  }

  return seec::Maybe<EventReference>();
}

/// Find the last Event in a range for which a predicate returns true.
/// \tparam PredT the type of the predicate.
/// \param Range the event range to search in.
/// \param Predicate the predicate to check the events with.
/// \return a seec::Maybe, which contains an EventReference for the last
///         Event in Range for which Predicate(Event) is true. If no such event
///         is found, the Maybe is unassigned.
template<typename PredT>
seec::Maybe<EventReference> rfind(EventRange Range, PredT Predicate) {
  if (Range.begin() == Range.end())
    return seec::Maybe<EventReference>();
  
  for (auto It = --(Range.end()); ; --It) {
    if (Predicate(*It)) {
      return seec::Maybe<EventReference>(It);
    }
    
    if (It == Range.begin())
      break;
  }

  return seec::Maybe<EventReference>();
}

/// Find the first Event in a range in a function for which a predicate returns
/// true.
template<typename PredT>
seec::Maybe<EventReference>
findInFunction(ThreadTrace const &Trace, EventRange Range, PredT Predicate) {
  for (auto It = Range.begin(); It != Range.end(); ++It) {
    switch (It->getType()) {
      case EventType::FunctionStart:
        // Skip any events belonging to child functions.
        {
          auto const &ChildStartEv = It.get<EventType::FunctionStart>();
          auto const ChildEndOffset = ChildStartEv.getEventOffsetEnd();
          
          /// Set It to the FunctionEnd (it will be incremented to the next
          /// event that is part of this function, by the loop).
          It = Trace.getReferenceToOffset(ChildEndOffset);
        }
        break;
        
      case EventType::FunctionEnd:
        return seec::Maybe<EventReference>();
        
      default:
        if (Predicate(*It))
          return seec::Maybe<EventReference>(It);
        break;
    }
  }
  
  return seec::Maybe<EventReference>();
}

/// Find the last Event in a range in a function for which a predicate returns
/// true.
template<typename PredT>
seec::Maybe<EventReference>
rfindInFunction(ThreadTrace const &Trace, EventRange Range, PredT Predicate) {
  if (Range.begin() == Range.end())
    return seec::Maybe<EventReference>();
  
  for (auto It = --(Range.end()); ; --It) {
    switch (It->getType()) {
      case EventType::FunctionStart:
        // This is the start of the active function, so no valid event was
        // found.
        return seec::Maybe<EventReference>();
      
      case EventType::FunctionEnd:
        // Skip any events belonging to child functions.
        {
          auto const &ChildEndEv = It.get<EventType::FunctionEnd>();
          auto const ChildStartOffset = ChildEndEv.getEventOffsetStart();
          
          // Set It to the FunctionStart (it will be decremented to the
          // previous event that is part of this function, by the loop).
          It = Trace.getReferenceToOffset(ChildStartOffset);
          
          // If the FunctionStart is outside of the Range, then no valid event
          // was found.
          if (It < Range.begin())
            return seec::Maybe<EventReference>();
        }
        break;
      
      default:
        if (Predicate(*It))
          return seec::Maybe<EventReference>(It);
        break;
    }
    
    // If this was the last event in the specified range, then stop searching.
    if (It == Range.begin())
      break;
  }
  
  return seec::Maybe<EventReference>();
}

/// For the first Event in a range for which the given predicate returns an
/// assigned Maybe, return that Maybe. Otherwise, return an empty Maybe. The
/// type of the returned value is equal to the return type of the supplied
/// predicate, but must contain an assigned() method.
/// \tparam PredT the type of the predicate.
/// \param Range the event range to search in.
/// \param Predicate the predicate to apply to the events.
template<typename PredT>
typename seec::FunctionTraits<PredT>::ReturnType
firstSuccessfulApply(EventRange Range, PredT Predicate) {
  for (auto &&Ev : Range) {
    auto Value = Predicate(Ev);
    if (Value.assigned())
      return Value;
  }

  return typename seec::FunctionTraits<PredT>::ReturnType {};
}

/// For the last Event in a range for which the given predicate returns an
/// assigned llvm::Optional, return that llvm::Optional. Otherwise, return an
/// unassigned llvm::Optional.
/// \tparam PredT the type of the predicate.
/// \param Range the event range to search in.
/// \param Predicate the predicate to apply to the events.
///
template<typename PredT>
typename seec::FunctionTraits<PredT>::ReturnType
lastSuccessfulApply(EventRange Range, PredT Predicate) {
  if (Range.begin() == Range.end())
    return typename seec::FunctionTraits<PredT>::ReturnType {};
  
  assert(Range.begin() < Range.end());
  
  for (auto It = --(Range.end()); ; --It) {
    auto Value = Predicate(*It);
    if (Value)
      return Value;
    
    if (It == Range.begin())
      break;
  }

  return typename seec::FunctionTraits<PredT>::ReturnType {};
}

/// @}


/// \name EventRange Helpers
/// @{

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

/// @}


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
