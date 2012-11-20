//===- Util/TemplateSequence.hpp ------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_TEMPLATESEQUENCE_HPP
#define SEEC_UTIL_TEMPLATESEQUENCE_HPP

namespace seec {

/// Compile-time utilities.
namespace ct {

/// \brief A compile-time sequence of ints.
template<int ...>
struct sequence_int {};

/// \brief Build a compile-time sequence of ints to cover a range.
/// Builds a sequence [Start, End) by recursively instantiating itself and
/// accumulating in the Sequence parameter pack.
template<int Start, int End, int ...Sequence>
struct generate_sequence_int
: public generate_sequence_int<Start, End - 1, End - 1, Sequence...> {};

/// \brief Build a compile-time sequence of ints to cover a range.
/// Base case of the recursion, create a sequence using all values that have
/// accumulated in the Sequence parameter pack.
template<int StartEnd, int ...Sequence>
struct generate_sequence_int<StartEnd, StartEnd, Sequence...> {
  typedef sequence_int<Sequence...> type;
};

} // namespace ct

} // namespace seec

#endif // SEEC_UTIL_TEMPLATESEQUENCE_HPP
