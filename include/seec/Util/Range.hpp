//===- include/seec/Util/Range.hpp ---------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_RANGE_HPP
#define SEEC_UTIL_RANGE_HPP

namespace seec {

/// A pair of iterators that can be used in a ranged for loop.
/// \tparam IterT 
template<typename IterT>
class Range
{
  IterT Begin;
  
  IterT End;
  
public:
  Range(IterT Begin, IterT End)
  : Begin(Begin),
    End(End)
  {}
  
  IterT begin() const { return Begin; }
  
  IterT end() const { return End; }
};

template<typename IterT>
Range<IterT> range(IterT Begin, IterT End) {
  return Range<IterT>(Begin, End);
}

} // namespace seec

#endif // SEEC_UTIL_RANGE_HPP
