//===- include/seec/DSA/Interval.hpp -------------------------------- C++ -===//
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

#ifndef SEEC_DSA_INTERVAL_HPP
#define SEEC_DSA_INTERVAL_HPP

#include <algorithm>
#include <cassert>
#include <type_traits>

namespace seec {

/// \brief Defines an interval of the form [ValueT, ValueT).
/// \tparam ValueT the type of value used for the boundaries of the interval.
///
template<typename ValueT>
class Interval {
  /// The first value that is a part of the interval.
  ValueT Start;

  /// The first value that is not a part of the interval.
  ValueT End;

  /// Constructor.
  Interval(ValueT StartValue, ValueT EndValue)
  noexcept
  : Start(StartValue),
    End(EndValue)
  {}

public:
  /// \name Constructors.
  /// @{

  /// \brief Copy constructor.
  ///
  Interval(Interval const &RHS) noexcept = default;

  /// \brief Construct a new interval by specifying the start and end.
  ///
  static Interval withStartEnd(ValueT StartValue, ValueT EndValue)
  noexcept
  {
    assert(EndValue >= StartValue);
    return Interval(StartValue, EndValue);
  }

  /// \brief Construct a new interval by specifying the start and length.
  ///
  static Interval withStartLength(ValueT StartValue, ValueT LengthValue)
  noexcept
  {
    assert(LengthValue >= 0);
    return Interval(StartValue, StartValue + LengthValue);
  }

  /// @}


  /// \name Assignment.
  /// @{

  /// \brief Copy assignment.
  ///
  Interval &operator=(Interval const &RHS) = default;

  /// @} (Assignment)


  /// \name Accessors.
  /// @{

  /// \brief Get the first value in the interval.
  ///
  ValueT const &start() const { return Start; }

  /// \brief Get the first value not in the interval.
  ///
  ValueT const &end() const { return End; }

  /// @} (Accessors)


  /// \name Mutators.
  /// @{

  /// \brief Set a new Start value for the interval.
  ///
  void setStart(ValueT Value) {
    assert(Value <= End);
    Start = Value;
  }

  /// \brief Set a new End value for the interval.
  ///
  void setEnd(ValueT Value) {
    assert(Value >= Start);
    End = Value;
  }

  /// \brief Set new Start and End values for the interval.
  ///
  void setStartEnd(ValueT StartValue, ValueT EndValue) {
    assert(EndValue >= StartValue);
    Start = StartValue;
    End = EndValue;
  }

  /// \brief Get a copy of this Interval with start equal to Value.
  ///
  Interval withStart(ValueT Value) const {
    assert(Value <= End);
    return withStartEnd(Value, End);
  }

  /// \brief Get a copy of this Interval with end equal to Value.
  ///
  Interval withEnd(ValueT Value) const {
    assert(Value >= Start);
    return withStartEnd(Start, Value);
  }

  /// @}


  /// \name Queries.
  /// @{

  /// \brief Get the length of the interval.
  ///
  ValueT length() const { return End - Start; }

  /// \brief Get the last value in the interval (for integral ValueT only).
  ///
  typename std::enable_if<std::is_integral<ValueT>::value, ValueT>::type
  last() const {
    if (Start != End)
      return End - 1;
    return End;
  }

  /// \brief Check if a value is contained in this interval.
  ///
  bool contains(ValueT Value) const {
    return Start <= Value && Value < End;
  }

  /// \brief Check if another interval is completely contained in this interval.
  ///
  bool contains(Interval const &Other) const {
    return Start <= Other.Start && End >= Other.End;
  }

  /// \brief Check if another interval intersects this interval.
  ///
  bool intersects(Interval const &Other) const {
    return Other.End > Start && Other.Start < End;
  }

  /// \brief Get the intersection of this interval with another interval.
  ///
  Interval<ValueT> intersection(Interval const &Other) const {
    // If there is no intersection, return an empty Interval.
    if (!intersects(Other))
      return Interval<ValueT>::withStartEnd(Start, Start);

    auto IntersectStart = std::max(Start, Other.Start);
    auto IntersectEnd = std::min(End, Other.End);

    return Interval<ValueT>::withStartEnd(IntersectStart, IntersectEnd);
  }
  
  /// \brief Check if this interval is equal to another interval.
  ///
  bool operator==(Interval const &RHS) const {
    return Start == RHS.Start && End == RHS.End;
  }
  
  /// \brief Check if this interval is not equal to another interval
  ///
  bool operator!=(Interval const &RHS) const {
    return Start != RHS.Start || End != RHS.End;
  }

  /// @} (Queries)
};

} // namespace seec

#endif // SEEC_DSA_INTERVAL_HPP
