//===- seec/Util/FunctionTraits.hpp --------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_FUNCTION_TRAITS_HPP_
#define _SEEC_UTIL_FUNCTION_TRAITS_HPP_

#include <cstring>

namespace seec {

/// \name Select a specific type from a list of types.
/// @{

/// Base template (undefined).
template<size_t Index, typename... Types>
struct ChooseTypeFromList;

/// \brief Select a specific type from a list of types.
///
/// The first argument is the index of the type to select, and the remaining
/// arguments are the types in the list. For example:
///   ChooseTypeFromList<1, int, char, std::string>::Type == int
///   ChooseTypeFromList<3, int, char, std::string>::Type == std::string
/// \tparam Index the index of the type to select (1-based).
/// \tparam T the first type in the list.
/// \tparam Types the remaining types in the list.
template<size_t Index, typename T, typename... Types>
struct ChooseTypeFromList<Index, T, Types...> {
  /// The Index-th type in the list.
  typedef typename ChooseTypeFromList<Index - 1, Types...>::Type Type;
};

/// \brief Specialization to select the first type from a list of types.
/// \tparam T the first type in the list.
/// \tparam Types the remaining types in the list.
template<typename T, typename... Types>
struct ChooseTypeFromList<1, T, Types...> {
  /// The first type in the list (T).
  typedef T Type;
};

/// @}


/// \name Get the type of a specific argument in a function type.
/// @{

/// \brief Select the Index-th type from the argument types of function F.
///
/// This general case template assumes that F is the type of some function-like
/// object (a functor or lambda), and deduces the argument types from the type
/// of &F::operator(), which is matched to one of this template's
/// specializations.
/// \tparam Index the index of the type to select (1-based).
/// \tparam F the type of the function whose arguments are being selected from.
template<size_t Index, typename F>
struct ChooseArgumentType {
private:
  typedef ChooseArgumentType<Index, decltype(&F::operator())> Chain;

public:
  /// The type of the Index-th argument for function F.
  typedef typename Chain::Type Type;
};

/// \brief Specialization for member function pointers.
/// \tparam Index the index of the type to select (1-based).
/// \tparam R the return type of the function.
/// \tparam C the type of the object containing the function.
/// \tparam Args the types of the function's arguments.
template<size_t Index, typename R, typename C, typename... Args>
struct ChooseArgumentType<Index, R (C::*)(Args...)> {
  /// The type of the Index-th argument in Args.
  typedef typename ChooseTypeFromList<Index, Args...>::Type Type;
};

/// \brief Specialization for const member function pointers.
/// \tparam Index the index of the type to select (1-based).
/// \tparam R the return type of the function.
/// \tparam C the type of the object containing the function.
/// \tparam Args the types of the function's arguments.
template<size_t Index, typename R, typename C, typename... Args>
struct ChooseArgumentType<Index, R (C::*)(Args...) const> {
  /// The type of the Index-th argument in Args.
  typedef typename ChooseTypeFromList<Index, Args...>::Type Type;
};

/// \brief Specialization for function pointers.
/// \tparam Index the index of the type to select (1-based).
/// \tparam R the return type of the function.
/// \tparam Args the types of the function's arguments.
template<size_t Index, typename R, typename... Args>
struct ChooseArgumentType<Index, R (*)(Args...)> {
  /// The type of the Index-th argument in Args.
  typedef typename ChooseTypeFromList<Index, Args...>::Type Type;
};

/// \brief Specialization for functions.
/// \tparam Index the index of the type to select (1-based).
/// \tparam R the return type of the function.
/// \tparam Args the types of the function's arguments.
template<size_t Index, typename R, typename... Args>
struct ChooseArgumentType<Index, R (Args...)> {
  /// The type of the Index-th argument in Args.
  typedef typename ChooseTypeFromList<Index, Args...>::Type Type;
};

/// @}


/// \name Get information about a function type.
/// @{

/// \brief Get information about a function type.
///
/// This template lets the user retrieve the return type and argument count of a
/// function or function-like object. This general case assumes that it is
/// matching an object (function object or lambda), and deduces the properties
/// of the object's operator() by using one of this template's specializations.
/// \tparam T the function type to retrieve information about.
template<typename T>
struct FunctionTraits {
private:
  typedef FunctionTraits<decltype(&T::operator())> Chain;

public:
  /// The return type of the function T.
  typedef typename Chain::ReturnType ReturnType;

  /// The number of arguments accepted by function T.
  static constexpr size_t ArgumentCount = Chain::ArgumentCount;
};

/// \brief Specialization to match member function pointers.
/// \tparam R the return type of the function.
/// \tparam C the type of the object containing the function.
/// \tparam Args the arguments accepted by the function.
template<typename R, typename C, typename... Args>
struct FunctionTraits<R (C::*)(Args...)> {
  /// The return type (R) of the function.
  typedef R ReturnType;

  /// The number of arguments (sizeof...(Args)) accepted by the function.
  static constexpr size_t ArgumentCount = sizeof...(Args);
};

/// \brief Specialization to match const member function pointers.
/// \tparam R the return type of the function.
/// \tparam C the type of the object containing the function.
/// \tparam Args the arguments accepted by the function.
template<typename R, typename C, typename... Args>
struct FunctionTraits<R (C::*)(Args...) const> {
  /// The return type (R) of the function.
  typedef R ReturnType;

  /// The number of arguments (sizeof...(Args)) accepted by the function.
  static constexpr size_t ArgumentCount = sizeof...(Args);
};

/// \brief Specialization to match function pointers.
/// \tparam R the return type of the function.
/// \tparam Args the arguments accepted by the function.
template<typename R, typename... Args>
struct FunctionTraits<R (*)(Args...)> {
  /// The return type (R) of the function.
  typedef R ReturnType;

  /// The number of arguments (sizeof...(Args)) accepted by the function.
  static constexpr size_t ArgumentCount = sizeof...(Args);
};

/// \brief Specialization to match functions.
/// \tparam R the return type of the function.
/// \tparam Args the arguments accepted by the function.
template<typename R, typename... Args>
struct FunctionTraits<R (Args...)> {
  /// The return type (R) of the function.
  typedef R ReturnType;

  /// The number of arguments (sizeof...(Args)) accepted by the function.
  static constexpr size_t ArgumentCount = sizeof...(Args);
};

/// @}

} // namespace seec

#endif // _SEEC_UTIL_FUNCTION_TRAITS_HPP_
