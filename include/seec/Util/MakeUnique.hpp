//===- seec/Util/MakeUnique.hpp ------------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_MAKE_UNIQUE_HPP_
#define _SEEC_UTIL_MAKE_UNIQUE_HPP_

#include <memory>
#include <new>

namespace seec {

template<typename T, typename PtrT = T, typename... Params>
std::unique_ptr<PtrT> makeUnique(Params&&... params) {
  return std::unique_ptr<PtrT>(
    new (std::nothrow) T(std::forward<Params>(params)...));
}

} // namespace seec

#endif // _SEEC_UTIL_MAKE_UNIQUE_HPP_
