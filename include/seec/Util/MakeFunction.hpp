//===- seec/Util/MakeFunction.hpp ----------------------------------- C++ -===//
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

#ifndef SEEC_UTIL_MAKEFUNCTION_HPP
#define SEEC_UTIL_MAKEFUNCTION_HPP

#include <functional>

namespace seec {

/// Implementation details for \c seec::make_function().
namespace detail {

template<typename T> struct remove_class {};

template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...)> { using type = R(A...); };

template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) const> { using type = R(A...); };

template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) volatile> { using type = R(A...); };

template<typename C, typename R, typename... A>
struct remove_class<R(C::*)(A...) const volatile> { using type = R(A...); };

template<typename T>
struct get_signature_impl {
private:
  using optype = decltype(&std::remove_reference<T>::type::operator());
public:
  using type = typename remove_class<optype>::type;
};

template<typename R, typename... A>
struct get_signature_impl<R(A...)> { using type = R(A...); };

template<typename R, typename... A>
struct get_signature_impl<R(&)(A...)> { using type = R(A...); };

template<typename R, typename... A>
struct get_signature_impl<R(*)(A...)> { using type = R(A...); };

template<typename T>
using get_signature = typename get_signature_impl<T>::type;

template<typename F>
using make_function_type = std::function<get_signature<F>>;

} // namespace detail

/// \brief Wrap a simple lambda in a \c std::function.
/// \tparam F the type of the lambda (or functor).
/// \param f the lambda (or functor).
/// \return a \c std::function that calls \c f.
/// from: http://stackoverflow.com/questions/11893141/inferring-the-call-signature-of-a-lambda-or-arbitrary-callable-for-make-functio
///
template<typename F>
detail::make_function_type<F> make_function(F &&f) {
  return detail::make_function_type<F>(std::forward<F>(f));
}

/// \brief Wrap a member function and an instance of its class as a
///        \c std::function.
/// \tparam C the class that the member function belongs to.
/// \tparam R the return type of the member function.
/// \tparam A the argument types of the member function.
/// \param Object the instance of the class that will be bound to.
/// \param Fn the member function that will be called.
/// \return a \c std::function that calls the member function \c Fn on the
///         instance \c Object.
///
template<typename C, typename R, typename... A>
std::function<R(A...)> make_function(C &Object, R (C::* Fn)(A...)) {
  return [&Object, Fn] (A... Args) { return (Object.*Fn)(Args...); };
}

/// \brief Wrap a member function and an instance of its class as a
///        \c std::function.
/// \tparam C the class that the member function belongs to.
/// \tparam R the return type of the member function.
/// \tparam A the argument types of the member function.
/// \param Object the instance of the class that will be bound to.
/// \param Fn the member function that will be called.
/// \return a \c std::function that calls the member function \c Fn on the
///         instance \c Object.
///
template<typename C, typename R, typename... A>
std::function<R(A...)> make_function(C *Object, R (C::* Fn)(A...)) {
  return [=] (A... Args) { return (Object->*Fn)(Args...); };
}

} // namespace seec

#endif // SEEC_UTIL_MAKEFUNCTION_HPP
