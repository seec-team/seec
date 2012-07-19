//===- seec/Util/TypeHandling.hpp ----------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_TYPE_HANDLING_HPP_
#define _SEEC_UTIL_TYPE_HANDLING_HPP_

namespace seec {

/// \name Remove pointers from a type.
/// @{

template<typename T>
struct Pointee { typedef T Type; };

template<typename T>
struct Pointee<T *> { typedef T Type; };

template<typename T>
struct Pointee<T * const> { typedef T Type; };

template<typename T>
struct StripPointers { typedef T Type; };

template<typename T>
struct StripPointers<T *> { typedef typename StripPointers<T>::Type Type; };

template<typename T>
struct StripPointers<T * const> {
  typedef typename StripPointers<T>::Type Type;
};

/// @}


/// \name Remove const from a type.
/// @{

template<typename T>
struct StripConst { typedef T Type; };

template<typename T>
struct StripConst<T const> { typedef T Type; };

/// @}


/// \name Remove (const) pointers, (const) references, and consts from a type.
/// @{

template<typename T>
struct StripAll { typedef T Type; };

template<typename T>
struct StripAll<T *> { typedef typename StripAll<T>::Type Type; };

template<typename T>
struct StripAll<T &> { typedef typename StripAll<T>::Type Type; };

template<typename T>
struct StripAll<T &&> { typedef typename StripAll<T>::Type Type; };

template<typename T>
struct StripAll<T const> { typedef typename StripAll<T>::Type Type; };

/// @}

} // namespace seec

#endif // _SEEC_UTIL_TYPE_HANDLING_HPP_
