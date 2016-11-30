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

#include <condition_variable>
#include <mutex>


/// \brief Controls access to a state.
///
class StateAccessToken
{
  /// Controls access to this token.
  mutable std::mutex AccessMutex;

  /// Indicates whether or not it is legal to access the state using this token.
  bool Valid;

  /// Number of clients accessing via this token.
  mutable unsigned Count;

  /// Used to notify that the count has reached zero.
  mutable std::condition_variable CountCV;

public:
  /// \brief Create a new, valid, access token.
  ///
  StateAccessToken()
  : AccessMutex(),
    Valid(true),
    Count(0),
    CountCV()
  {}

  /// \brief Invalidates this token before it is destroyed.
  ///
  ~StateAccessToken()
  {
    invalidate();
  }

  /// \brief
  ///
  bool acquireAccess() const
  {
    std::lock_guard<std::mutex> Lock{AccessMutex};

    if (!Valid)
      return false;

    ++Count;

    return true;
  }

  /// \brief
  ///
  void releaseAccess() const
  {
    std::unique_lock<std::mutex> Lock{AccessMutex};

    --Count;

    if (Count == 0) {
      Lock.unlock();
      CountCV.notify_all();
    }
  }

  /// \brief
  ///
  class StateAccess {
    StateAccessToken const &Token; ///<
    bool Valid; ///<

  public:
    /// \brief
    ///
    StateAccess(StateAccessToken const &WithToken)
    : Token(WithToken),
      Valid(Token.acquireAccess())
    {}

    // Copying is not allowed.
    StateAccess(StateAccess const &) = delete;
    StateAccess &operator=(StateAccess const &) = delete;

    // Moving is OK.
    StateAccess(StateAccess &&) = default;
    StateAccess &operator=(StateAccess &&) = default;

    /// \brief
    ///
    void release()
    {
      if (Valid) {
        Token.releaseAccess();
        Valid = false;
      }
    }

    /// \brief
    ///
    ~StateAccess()
    {
      release();
    }

    /// \brief
    ///
    operator bool() const { return Valid; }
  };

  /// \brief Acquire access to read from the associated state.
  ///
  StateAccess getAccess() const { return StateAccess(*this); }

  /// \brief Invalidate this token.
  ///
  void invalidate() {
    std::unique_lock<std::mutex> Lock{AccessMutex};

    if (!Valid)
      return;

    // Wait until there are no accessors.
    if (Count != 0)
      CountCV.wait(Lock);

    Valid = false;
  }
};


#endif // SEEC_TRACE_VIEW_STATEACCESSTOKEN_HPP
