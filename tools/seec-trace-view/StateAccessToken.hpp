//===- tools/seec-trace-view/StateAccessToken.hpp -------------------------===//
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

#ifndef SEEC_TRACE_VIEW_STATEACCESSTOKEN_HPP
#define SEEC_TRACE_VIEW_STATEACCESSTOKEN_HPP

#include <mutex>


/// \brief Controls access to a state.
///
class StateAccessToken
{
  /// Indicates whether or not it is legal to access the state using this token.
  bool Valid;
  
  /// Controls access to the state.
  mutable std::mutex AccessMutex;
  
public:
  /// \brief Create a new, valid, access token.
  ///
  StateAccessToken()
  : Valid{true}
  {}
  
  /// \brief Acquire access to read from the associated state.
  ///
  std::unique_lock<std::mutex> getAccess() const {
    std::unique_lock<std::mutex> Lock {AccessMutex};
    
    if (!Valid)
      Lock.unlock();
    
    return Lock;
  }
  
  /// \brief Invalidate this token.
  ///
  void invalidate() {
    // Ensure that no other threads are accessing when we invalidate.
    std::lock_guard<std::mutex> Lock{AccessMutex};
    Valid = false;
  }
};


#endif // SEEC_TRACE_VIEW_STATEACCESSTOKEN_HPP
