//===- seec/Util/Reverse.hpp ---------------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_REVERSE_HPP_
#define _SEEC_UTIL_REVERSE_HPP_

namespace seec {

/// Adapt container types to be reversed for use in range for loops.
template<typename T>
class ReverseAdaptor {
private:
  T &Container;

public:
  ReverseAdaptor(T &Container)
  : Container(Container)
  {}

  typename T::reverse_iterator begin() { return Container.rbegin(); }

  typename T::reverse_iterator end() { return Container.rend(); }
};

/// Adapt const container types to be reversed for use in range for loops.
template<typename T>
class ReverseAdaptorConst {
private:
  T const &Container;

public:
  ReverseAdaptorConst(T const &Container)
  : Container(Container)
  {}

  typename T::const_reverse_iterator begin() { return Container.rbegin(); }

  typename T::const_reverse_iterator end() { return Container.rend(); }
};

template<typename T>
ReverseAdaptor<T> reverse(T &Container) {
  return ReverseAdaptor<T>(Container);
}

template<typename T>
ReverseAdaptorConst<T> reverse(T const &Container) {
  return ReverseAdaptorConst<T>(Container);
}

} // namespace seec

#endif // _SEEC_UTIL_REVERSE_HPP_
