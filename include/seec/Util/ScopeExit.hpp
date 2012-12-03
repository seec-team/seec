//===- include/seec/Util/ScopeExit.hpp ------------------------------ C++ -===//
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

#ifndef SEEC_UTIL_SCOPEEXIT_HPP
#define SEEC_UTIL_SCOPEEXIT_HPP

#include <functional>

namespace seec {
  
class ScopeExit {
  std::function<void()> Func;
  
  // don't allow copying
  ScopeExit(ScopeExit const &) = delete;
  ScopeExit &operator=(ScopeExit const &) = delete;

public:
  explicit ScopeExit(std::function<void()> CallFunction)
  : Func(CallFunction)
  {}
  
  ~ScopeExit() {
    if (Func)
      Func();
  }
  
  void disable() { Func = nullptr; }
};

} // namespace seec

#endif // SEEC_UTIL_SCOPEEXIT_HPP
