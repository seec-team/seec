//===- seec/Util/Reverse.hpp ---------------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_REVERSE_HPP
#define SEEC_UTIL_REVERSE_HPP

namespace seec {

/// \brief Adapt container types to be reversed for begin() and end().
///
/// This template class can be used to create a "reversed" view of a container,
/// for use in range for loops and other places that support only forward
/// iteration using begin() and end().
///
/// \tparam The type of container.
///
template<typename T>
class ReverseAdaptor {
private:
  T &Container; ///< The container being adapted.

public:
  /// \brief Construct a new ReverseAdaptor for the given container.
  ReverseAdaptor(T &Container)
  : Container(Container)
  {}

  /// \brief Get the beginning reverse iterator for the container.
  /// \return Container.rbegin()
  typename T::reverse_iterator begin() { return Container.rbegin(); }

  /// \brief Get the ending reverse iterator for the container.
  /// \return Container.rend()
  typename T::reverse_iterator end() { return Container.rend(); }
};

/// \brief Adapt const container types to be reversed for begin() and end().
///
/// This template class can be used to create a "reversed" view of a container,
/// for use in range for loops and other places that support only forward
/// iteration using begin() and end().
///
/// \tparam The type of container.
///
template<typename T>
class ReverseAdaptorConst {
private:
  T const &Container; ///< The container being adapted.

public:
  /// \brief Construct a new ReverseAdaptorConst for the given const container.
  ReverseAdaptorConst(T const &Container)
  : Container(Container)
  {}

  /// \brief Get the beginning reverse iterator for the container.
  /// \return Container.rbegin()
  typename T::const_reverse_iterator begin() const {
    return Container.rbegin();
  }

  /// \brief Get the ending reverse iterator for the container.
  /// \return Container.rend()
  typename T::const_reverse_iterator end() const {
    return Container.rend();
  }
};

/// \brief Get a "reversed" view of the given container.
/// \tparam The type of container.
/// \param Container a reference to the container.
/// \return A ReverseAdaptor<T> for Container.
template<typename T>
ReverseAdaptor<T> reverse(T &Container) {
  return ReverseAdaptor<T>(Container);
}

/// \brief Get a "reversed" view of the given container.
/// \tparam The type of container.
/// \param Container a const reference to the container.
/// \return A ReverseAdaptorConst<T> for Container.
template<typename T>
ReverseAdaptorConst<T> reverse(T const &Container) {
  return ReverseAdaptorConst<T>(Container);
}

} // namespace seec

#endif // SEEC_UTIL_REVERSE_HPP
