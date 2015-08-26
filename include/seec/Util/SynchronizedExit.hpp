//===- Util/SynchronizedExit.hpp ------------------------------------ C++ -===//
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

#ifndef SEEC_UTIL_SYNCHRONIZEDEXIT_HPP
#define SEEC_UTIL_SYNCHRONIZEDEXIT_HPP

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace seec {


/// \brief Support synchronizing std::exit() amongst a group of participating
///        threads.
///
class SynchronizedExit {
  std::mutex Access;
  
  uint32_t NumThreads;
  
  uint32_t StoppedThreads;

  std::thread::id StopMaster;
  
  std::condition_variable AllThreadsStopped;
  
  std::condition_variable StopCancelled;
  
  bool initiateStop(std::unique_lock<std::mutex> &Lock);
  
  void joinStop(std::unique_lock<std::mutex> &Lock);
  
  void cancelStop();

public:
  /// \brief Constructor.
  ///
  SynchronizedExit()
  : Access(),
    NumThreads(0),
    StoppedThreads(0),
    StopMaster(),
    AllThreadsStopped(),
    StopCancelled()
  {}
  
  SynchronizedExit(SynchronizedExit const &) = delete;
  
  SynchronizedExit &operator=(SynchronizedExit const &) = delete;
  
  /// \brief Notify that this thread has started.
  ///
  void threadStart();
  
  /// \brief Notify that this thread is terminating.
  ///
  void threadFinish();
  
  /// \brief Can be used to resume execution after an all-stop.
  class StopCanceller {
    friend class SynchronizedExit;

    SynchronizedExit &SE;

    bool Stopped;

    bool Restarted;

    StopCanceller(SynchronizedExit &TheSE, bool const WithStopped)
    : SE(TheSE),
      Stopped(WithStopped),
      Restarted(false)
    {}

  public:
    bool wasStopped() const { return Stopped; }

    bool cancelStop();
  };

  /// \brief Stop all threads and then return.
  ///
  StopCanceller stopAll();
  
  /// \brief Stop all threads and call std::abort().
  ///
  void abort();
  
  /// \brief Stop all threads and call std::exit().
  ///
  void exit(int ExitCode);
  
  /// \brief Check if we should join an active stop.
  ///
  void check() {
    std::unique_lock<std::mutex> Lock(Access);

    if (StoppedThreads) {
      joinStop(Lock);
    }
  }
};


/// \brief RAII object for supporting synchronized exits.
class SupportSynchronizedExit {
  SynchronizedExit &SE;
  
public:
  SupportSynchronizedExit(SynchronizedExit &SE)
  : SE(SE)
  {
    SE.threadStart();
  }
  
  ~SupportSynchronizedExit()
  {
    SE.threadFinish();
  }
  
  SynchronizedExit &getSynchronizedExit() const { return SE; }
};


} // namespace seec

#endif // SEEC_UTIL_SYNCHRONIZEDEXIT_HPP
