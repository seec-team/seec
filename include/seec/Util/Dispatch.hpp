//===- seec/Util/Dispatch.hpp ------------------------------------- C++11 -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// TODO: Add logic to:
///  2) Check for a match to the same underlying type (e.g. change reference
///      or add const).
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_DISPATCH_HPP
#define SEEC_UTIL_DISPATCH_HPP

#include "seec/Util/FunctionTraits.hpp"
#include "seec/Util/Maybe.hpp"

#include <type_traits>

namespace seec {
  
//------------------------------------------------------------------------------
// DispatchFlags, getDispatchFlagSet(), getDefaultDispatchFlagSet()
//------------------------------------------------------------------------------

/// Contains flags for controlling the behaviour of the dispatch() function.
enum class DispatchFlags : uint32_t {
  MatchSame = 1,
  MatchConvertible = 2,
  ErrorOnUnmatched = 4,
  MultipleReturnTypes = 8
};

/// Contains implementation details for the dispatch() function.
namespace dispatch_impl {

template<DispatchFlags... Flags>
struct DispatchFlagSet {
  static constexpr uint32_t getValue() {
    return 0;
  }
};

template<DispatchFlags Flag, DispatchFlags... Flags>
struct DispatchFlagSet<Flag, Flags...> {
  static constexpr uint32_t getValue() {
    return static_cast<uint32_t>(Flag) | DispatchFlagSet<Flags...>::getValue();
  }
};

} // namespace dispatch_impl (in seec)

template<DispatchFlags... Flags>
constexpr uint32_t getDispatchFlagSet() {
  return seec::dispatch_impl::DispatchFlagSet<Flags...>::getValue();
}

constexpr uint32_t getDefaultDispatchFlagSet() {
  return getDispatchFlagSet<DispatchFlags::MatchSame,
                            DispatchFlags::MatchConvertible>();
}

constexpr bool dispatchFlagSetHasFlag(uint32_t Set, DispatchFlags Flag) {
  return (Set & static_cast<uint32_t>(Flag)) != 0;
}

/// Contains implementation details for the dispatch() function.
namespace dispatch_impl {

//------------------------------------------------------------------------------
// indexOfPerfectMatch
//------------------------------------------------------------------------------

template<std::size_t CurrentIndex,
         typename T>
constexpr std::size_t indexOfPerfectMatch() {
  return 0;
}

template<std::size_t CurrentIndex,
         typename T,
         typename PredT,
         typename... PredTs>
constexpr
typename std::enable_if<
  std::is_same<T, typename ChooseArgumentType<1, PredT>::Type>::value,
  std::size_t>::type
indexOfPerfectMatch() {
  return CurrentIndex;
}

template<std::size_t CurrentIndex,
         typename T,
         typename PredT,
         typename... PredTs>
constexpr
typename std::enable_if<
  !std::is_same<T, typename ChooseArgumentType<1, PredT>::Type>::value,
  std::size_t>::type
indexOfPerfectMatch() {
  return indexOfPerfectMatch<CurrentIndex + 1, T, PredTs...>();
}

/// Get the 1-based index of a predicate in PredTs that accepts the exact type
/// T, if such a predicate exists. Otherwise, return 0.
template<typename T, typename... PredTs>
constexpr std::size_t indexOfPerfectMatch() {
  return indexOfPerfectMatch<1, T, PredTs...>();
}


//------------------------------------------------------------------------------
// indexOfConvertible
//------------------------------------------------------------------------------

template<std::size_t CurrentIndex,
         typename T>
constexpr std::size_t indexOfConvertible() {
  return 0;
}

template<std::size_t CurrentIndex,
         typename T,
         typename PredT,
         typename... PredTs>
constexpr
typename std::enable_if<
  std::is_convertible<T, typename ChooseArgumentType<1, PredT>::Type>::value,
  std::size_t>::type
indexOfConvertible() {
  return CurrentIndex;
}

template<std::size_t CurrentIndex,
         typename T,
         typename PredT,
         typename... PredTs>
constexpr
typename std::enable_if<
  !std::is_convertible<T, typename ChooseArgumentType<1, PredT>::Type>::value,
  std::size_t>::type
indexOfConvertible() {
  return indexOfConvertible<CurrentIndex + 1, T, PredTs...>();
}

/// Get the 1-based index of a predicate in PredTs that accepts a type that
/// T can be implicitly converted to, if such a predicate exists. Otherwise,
/// return 0.
template<typename T, typename... PredTs>
constexpr std::size_t indexOfConvertible() {
  return indexOfConvertible<1, T, PredTs...>();
}


//------------------------------------------------------------------------------
// ReturnType
//------------------------------------------------------------------------------

/// Base case of ReturnType. In this case, the return type is a Maybe which
/// contains an element for the return type of each predicate, in the order
/// that the predicates were passed in. For example, if there are three
/// predicates with return types [char, int, std::string], then the return type
/// will be Maybe<char, int, std::string>.
template<uint32_t Flags, typename... PredTs>
struct ReturnType {
  typedef seec::Maybe<typename FunctionTraits<PredTs>::ReturnType...> type;
};

/// Specialization of ReturnType for when the MultipleReturnTypes flag is
/// disabled. In this case, the return type is a Maybe<RetT>, where RetT is the
/// return type of PredT (the first predicate). All remaining predicates must
/// have return type RetT, or a type that is implicitly convertible to RetT.
template<uint32_t Flags, typename PredT, typename... PredTs>
struct ReturnType
  <
  Flags,
  typename std::enable_if<
    !dispatchFlagSetHasFlag(Flags, DispatchFlags::MultipleReturnTypes)
  >::type,
  PredT,
  PredTs...>
{
  typedef typename FunctionTraits<PredT>::ReturnType PredTRetT;
  typedef seec::Maybe<PredTRetT> type;
};

//------------------------------------------------------------------------------
// doDispatch()
//------------------------------------------------------------------------------

/// Specialization of doDispatch for Index == 0. Returns a default-constructed
/// RetT.
template<uint32_t Flags,
         std::size_t Index,
         std::size_t MaybeIndex,
         typename RetT,
         typename T,
         typename... PredTs>
typename std::enable_if<
  (Index == 0),
  RetT>::type
doDispatch(T &&Object, PredTs &&...Preds) {
  static_assert(!dispatchFlagSetHasFlag(Flags, DispatchFlags::ErrorOnUnmatched),
                "Failed to match dispatch() object to a predicate.");
  
  return RetT();
}

/// Specialization of doDispatch for Index == 1.
template<uint32_t Flags,
         std::size_t Index,
         std::size_t MaybeIndex,
         typename RetT,
         typename T,
         typename PredT,
         typename... PredTs>
typename std::enable_if<
  (Index == 1),
  RetT>::type
doDispatch(T &&Object, PredT &&Pred, PredTs &&...Preds) {
  return RetT::template construct<MaybeIndex>(Pred(std::forward<T>(Object)));
}

/// Specialization of doDispatch for Index > 1. Removes the head from the list
/// and calls doDispatch on the remaining list items, with Index = Index - 1.
template<uint32_t Flags,
         std::size_t Index,
         std::size_t MaybeIndex,
         typename RetT,
         typename T,
         typename PredT,
         typename... PredTs>
typename std::enable_if<
  (Index > 1),
  RetT>::type
doDispatch(T &&Object, PredT &&Pred, PredTs &&...Preds) {
  return doDispatch<Flags,
                    Index - 1,
                    MaybeIndex,
                    RetT>
                    (std::forward<T>(Object),
                     std::forward<PredTs>(Preds)...);
}

} // namespace dispatch_impl (in seec)

/// Dispatch an object to one of a number of predicates, based on the type of
/// the object and the type of object that the predicates expect, and get the
/// return value.
template<uint32_t Flags, typename T, typename... PredTs>
typename seec::dispatch_impl::ReturnType<Flags, PredTs...>::type
dispatch(T &&Object, PredTs &&...Preds) {
  typedef typename seec::dispatch_impl::ReturnType<Flags, PredTs...>::type RetT;
  
  if (dispatchFlagSetHasFlag(Flags, DispatchFlags::MatchSame)) {
    constexpr auto Index
      = seec::dispatch_impl::indexOfPerfectMatch<T, PredTs...>();
    
    if (Index) {
      constexpr auto MaybeIndex
        = dispatchFlagSetHasFlag(Flags, DispatchFlags::MultipleReturnTypes)
        ? Index - 1
        : 0;
      
      return seec::dispatch_impl::doDispatch<Flags,
                                             Index,
                                             MaybeIndex,
                                             RetT>
                                            (std::forward<T>(Object),
                                             std::forward<PredTs>(Preds)...);
    }
  }
  
  if (dispatchFlagSetHasFlag(Flags, DispatchFlags::MatchConvertible)) {
    constexpr auto Index
      = seec::dispatch_impl::indexOfConvertible<T, PredTs...>();
    
    if (Index) {
      constexpr auto MaybeIndex
        = dispatchFlagSetHasFlag(Flags, DispatchFlags::MultipleReturnTypes)
        ? Index - 1
        : 0;
      
      return seec::dispatch_impl::doDispatch<Flags,
                                             Index,
                                             MaybeIndex,
                                             RetT>
                                            (std::forward<T>(Object),
                                             std::forward<PredTs>(Preds)...);
    }
  }
  
  return RetT();
}

/// Calls dispatch() using default flags.
template<typename T, typename... PredTs>
typename seec::dispatch_impl::ReturnType<getDefaultDispatchFlagSet(),
                                         PredTs...>::type
dispatch(T &&Object, PredTs &&...Preds) {
  return dispatch<getDefaultDispatchFlagSet()>(std::forward<T>(Object),
                                               std::forward<PredTs>(Preds)...);
}


} // namespace seec

#endif // SEEC_UTIL_DISPATCH_HPP
