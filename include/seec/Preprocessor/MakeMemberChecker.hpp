//===- Preprocessor/MakeMemberChecker.hpp ------------------------- C++11 -===//
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

#ifndef SEEC_PREPROCESSOR_MAKEMEMBERCHECKER_H
#define SEEC_PREPROCESSOR_MAKEMEMBERCHECKER_H

#define SEEC_PP_MAKE_MEMBER_CHECKER(NAME, MEMBER_NAME)                         \
template<typename ClassT, typename MemberT>                                    \
class NAME {                                                                   \
  template<typename T, typename U = decltype(T::MEMBER_NAME)>                  \
  static constexpr                                                             \
  typename std::enable_if<std::is_same<MemberT, U>::value, bool>::type         \
  check(int) { return true; }                                                  \
  template<typename T>                                                         \
  static constexpr bool check(...) { return false; }                           \
public:                                                                        \
  static bool const value = check<ClassT>(0);                                  \
};

#endif // SEEC_PREPROCESSOR_MAKEMEMBERCHECKER_H
