//===- Preprocessor/MakeMemberFnChecker.hpp ----------------------- C++11 -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_PREPROCESSOR_MAKEMEMBERFNCHECKER_H
#define SEEC_PREPROCESSOR_MAKEMEMBERFNCHECKER_H

#define SEEC_PP_MAKE_MEMBER_FN_CHECKER(NAME, FUNC_NAME)                        \
template<typename T, typename Sig>                                             \
class NAME {                                                                   \
  template<typename U, U> struct type_check;                                   \
  template<typename U> static constexpr bool                                   \
  check(type_check<Sig, &U::FUNC_NAME> *) { return true; }                     \
  template<typename> static constexpr bool check(...) { return false; }        \
public:                                                                        \
  static bool const value = check<T>(nullptr);                                 \
};

#endif // SEEC_PREPROCESSOR_MAKEMEMBERFNCHECKER_H
