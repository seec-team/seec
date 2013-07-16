//===- include/seec/Util/Range.hpp ---------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_RANGE_HPP
#define SEEC_UTIL_RANGE_HPP

#include <utility>

namespace seec {

/// \brief A pair of iterators that can be used in a ranged for loop.
/// \tparam IterT The type of iterator used in the range.
template<typename IterT>
class Range
{
  IterT Begin; ///< Iterator for the first element in the range.

  IterT End; ///< Iterator for the first element after the range.

public:
  /// \brief Construct a new range.
  /// \param Begin Iterator for the first element in the range.
  /// \param End Iterator for the first element after the range.
  Range(IterT Begin, IterT End)
  : Begin(Begin),
    End(End)
  {}

  /// \brief Get an iterator for the first element in the range.
  IterT begin() const { return Begin; }

  /// \brief Get an iterator for the first element after the range.
  IterT end() const { return End; }
};

/// Get a new range.
/// \tparam IterT The type of iterator used in the range.
/// \param Begin Iterator to the first element in the range.
/// \param End Iterator to the first element after the range.
template<typename IterT>
Range<IterT> range(IterT Begin, IterT End) {
  return Range<IterT>(Begin, End);
}

/// \brief Get a range covering a C array.
///
template<typename ElemT, unsigned ElemN>
Range<ElemT *> range(ElemT(&Array)[ElemN]) {
  return Range<ElemT *>(Array, Array + ElemN);
}

/// \brief Get a range from a pair.
///
template<typename IterT>
Range<IterT> range(std::pair<IterT, IterT> const &Iters) {
  return Range<IterT>(Iters.first, Iters.second);
}

} // namespace seec

#endif // SEEC_UTIL_RANGE_HPP
