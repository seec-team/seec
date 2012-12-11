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

namespace seec {


template<typename FunT>
class ScopeExit {
  FunT Function;
  
  bool Active;
  
public:
  ScopeExit() = delete;
  
  explicit ScopeExit(FunT CallFunction)
  : Function(std::move(CallFunction)),
    Active(true)
  {}
  
  ScopeExit(ScopeExit const &) = delete;
  
  ScopeExit &operator=(ScopeExit const &) = delete;
  
  ScopeExit(ScopeExit &&Other)
  : Function(std::move(Other.Function)),
    Active(Other.Active)
  {
    Other.disable();
  }
  
  ~ScopeExit() {
    if (Active)
      Function();
  }
  
  void disable() { Active = false; }
};


template<typename FunT>
ScopeExit<FunT> scopeExit(FunT Function) {
  return ScopeExit<FunT>(std::move(Function));
}


} // namespace seec

#endif // SEEC_UTIL_SCOPEEXIT_HPP
