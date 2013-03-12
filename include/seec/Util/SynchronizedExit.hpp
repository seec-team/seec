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

namespace seec {


/// \brief Support synchronizing std::exit() amongst a group of participating
///        threads.
///
class SynchronizedExit {
  std::mutex Access;
  
  bool ExitCalled;
  
  uint32_t NumThreads;
  
  uint32_t StoppedThreads;
  
  std::condition_variable AllThreadsStopped;
  
  std::condition_variable StopCancelled;
  
  void initiateStop();
  
  void joinStop(std::unique_lock<std::mutex> &Lock);
  
public:
  /// \brief Constructor.
  ///
  SynchronizedExit()
  : Access(),
    ExitCalled(false),
    NumThreads(0),
    StoppedThreads(0),
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
  
  /// \brief Stop all threads and then return.
  ///
  void stopAll();
  
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
    
    if (!StoppedThreads) // No stop is active.
      return;
      
    if (ExitCalled)
      return;
    
    joinStop(Lock);
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
