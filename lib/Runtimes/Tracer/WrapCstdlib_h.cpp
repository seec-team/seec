//===- lib/Runtimes/Tracer/WrapCstdlib_h.cpp ------------------------------===//
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

#include "SimpleWrapper.hpp"
#include "Tracer.hpp"

#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Support/CallSite.h"

#include <cstdlib>
#include <stack>


namespace seec {


/// \brief Stop all other threads and write trace information.
///
/// This prepares us to safely terminate the process.
///
void stopThreadsAndWriteTrace() {
  auto &ProcessEnv = seec::trace::getProcessEnvironment();
  auto &ProcessListener = ProcessEnv.getProcessListener();
  
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &ThreadListener = ThreadEnv.getThreadListener();
  
  // Interact with the thread listener's notification system.
  ThreadListener.enterNotification();
  
  // Stop all of the other threads.
  auto const &SupportSyncExit = ThreadListener.getSupportSynchronizedExit();
  SupportSyncExit.getSynchronizedExit().stopAll();
  
  // TODO: Write an event for this Instruction.
  
  // Write out the trace information (if tracing is enabled).
  auto const TraceEnabled = ProcessListener.traceEnabled();
  
  if (TraceEnabled) {
    ProcessListener.traceWrite();
    ProcessListener.traceFlush();
    ProcessListener.traceClose();
    
    for (auto const ThreadListenerPtr : ProcessListener.getThreadListeners()) {
      ThreadListenerPtr->traceWrite();
      ThreadListenerPtr->traceFlush();
      ThreadListenerPtr->traceClose();
    }
  }
}


/// Functions to call during exit().
static std::stack<void (*)()> AtExitFunctions;

/// Controls access to AtExitFunctions.
static std::mutex AtExitFunctionsMutex;

/// Functions to call during quick_exit().
static std::stack<void (*)()> AtQuickExitFunctions;

/// Controls access to AtQuickExitFunctions.
static std::mutex AtQuickExitFunctionsMutex;


/// \brief Implement a checking bsearch.
///
class BinarySearchImpl {
  /// The thread performing the sort.
  seec::trace::TraceThreadListener &ThreadListener;
  
  /// The memory checker.
  seec::trace::CStdLibChecker Checker;
  
  /// Pointer to the key.
  char const * const Key;
  
  /// Pointer to the start of the array.
  char const * const Array;
  
  /// Number of elements in the array.
  std::size_t const ElementCount;
  
  /// Size of each element.
  std::size_t const ElementSize;
  
  /// Comparison function.
  int (* const Compare)(char const *, char const *);
  
  /// \brief Acquire memory lock and ensure that memory is accessible.
  ///
  bool acquireMemory()
  {
    ThreadListener.enterNotification();
    ThreadListener.acquireGlobalMemoryReadLock();
    ThreadListener.acquireDynamicMemoryLock();
    
    // We check for "copy", because it doesn't require initialization, and
    // doesn't require read permission. We don't need the entire array to be
    // initialized, because the initialization that is *required* will be
    // checked in the comparison function.
    
    return Checker.checkMemoryExistsAndAccessibleForParameter
                   (0,
                    reinterpret_cast<uintptr_t>(Key),
                    ElementSize,
                    seec::runtime_errors::format_selects::MemoryAccess::Copy)
           &&
           Checker.checkMemoryExistsAndAccessibleForParameter
                   (1,
                    reinterpret_cast<uintptr_t>(Array),
                    ElementCount * ElementSize,
                    seec::runtime_errors::format_selects::MemoryAccess::Copy);
  }
  
  /// \brief Release memory lock.
  ///
  void releaseMemory()
  {
    ThreadListener.exitPostNotification();
  }
  
  /// \brief Get a pointer to an element in the array.
  ///
  char const *getElement(std::size_t Index) {
    assert(Index < ElementCount);
    
    return Array + (Index * ElementSize);
  }
  
  /// \brief Perform the binary search.
  char const *bsearch() {
    if (!ElementCount)
      return nullptr;
    
    std::size_t Min = 0;
    std::size_t Max = ElementCount - 1;
    
    while (Min <= Max) {
      std::size_t const Mid = Min + ((Max - Min) / 2);
      
      // Compare the midpoint with the key.
      releaseMemory();
      
      auto const Comparison = Compare(getElement(Mid), Key);
      
      if (!acquireMemory())
        return nullptr;
      
      if (Comparison < 0) // Mid-point is less than Key
        Min = Mid + 1;
      else if (Comparison > 0) { // Mid-point is greater than Key
        if (Mid == 0)
          return nullptr;
        
        Max = Mid - 1;
      }
      else // Mid-point is equal to Key
        return getElement(Mid);
    }
    
    return nullptr;
  }
  
public:
  /// \brief Constructor.
  ///
  BinarySearchImpl(seec::trace::ThreadEnvironment &WithThread,
                   char const * const ForKey,
                   char const * const ForArray,
                   std::size_t const WithElementCount,
                   std::size_t const WithElementSize,
                   int (* const WithCompare)(char const *, char const *))
  : ThreadListener(WithThread.getThreadListener()),
    Checker(WithThread.getThreadListener(),
            WithThread.getInstructionIndex(),
            seec::runtime_errors::format_selects::CStdFunction::bsearch),
    Key(ForKey),
    Array(ForArray),
    ElementCount(WithElementCount),
    ElementSize(WithElementSize),
    Compare(WithCompare)
  {}
  
  /// \brief Perform the quicksort.
  ///
  void *operator()()
  {
    acquireMemory();
    
    auto const Result = bsearch();
    
    releaseMemory();
    
    // const_cast due to the C standard.
    return const_cast<char *>(Result);
  }
};


/// \brief Implement a recording quicksort.
///
class QuickSortImpl {
  /// The thread performing the sort.
  seec::trace::TraceThreadListener &ThreadListener;
  
  /// The memory checker.
  seec::trace::CStdLibChecker Checker;
  
  /// Pointer to the start of the array.
  char * const Array;
  
  /// Number of elements in the array.
  std::size_t const ElementCount;
  
  /// Size of each element.
  std::size_t const ElementSize;
  
  /// Comparison function.
  int (* const Compare)(char const *, char const *);
  
  /// \brief Acquire memory lock and ensure that memory is accessible.
  ///
  bool acquireMemory()
  {
    ThreadListener.enterNotification();
    ThreadListener.acquireGlobalMemoryWriteLock();
    ThreadListener.acquireDynamicMemoryLock();
    
    return Checker.checkMemoryExistsAndAccessibleForParameter
                   (0,
                    reinterpret_cast<uintptr_t>(Array),
                    ElementCount * ElementSize,
                    seec::runtime_errors::format_selects::MemoryAccess::Write);
  }
  
  /// \brief Release memory lock.
  ///
  void releaseMemory()
  {
    ThreadListener.exitPostNotification();
  }
  
  /// \brief Get a pointer to an element in the array.
  ///
  char *getElement(std::size_t Index) {
    assert(Index < ElementCount);
    
    return Array + (Index * ElementSize);
  }
  
  /// \brief Swap two elements in the array.
  ///
  void swap(std::size_t IndexA, std::size_t IndexB) {
    assert((IndexA < ElementCount) && (IndexB < ElementCount));
    
    if (IndexA == IndexB)
      return;
    
    auto const ElemA = getElement(IndexA);
    auto const ElemB = getElement(IndexB);
    
    char TempValue[ElementSize];
    
    memcpy(TempValue, ElemA,     ElementSize);
    memcpy(ElemA,     ElemB,     ElementSize);
    memcpy(ElemB,     TempValue, ElementSize);
    
    // Create a new thread time for this "step" of the sort, and record the
    // updated memory states.
    ThreadListener.incrementThreadTime();
    ThreadListener.recordUntypedState(ElemA, ElementSize);
    ThreadListener.recordUntypedState(ElemB, ElementSize);
  }
  
  /// \brief Partition.
  ///
  seec::util::Maybe<std::size_t>
  partition(std::size_t const Left,
            std::size_t const Right,
            std::size_t const Pivot)
  {
    // Move the pivot to the end.
    swap(Pivot, Right);
    
    // Shift all elements "less than" the pivot to the left side.
    std::size_t StoreIndex = Left;
    
    for (std::size_t i = Left; i < Right; ++i) {
      // Release memory so that the compare function can access it.
      releaseMemory();
      
      // Compare the current element to the pivot.
      auto const Comparison = Compare(getElement(i), getElement(Right));
      
      // Lock memory and ensure that the compare function hasn't deallocated it.
      if (!acquireMemory())
        return seec::util::Maybe<std::size_t>();
            
      if (Comparison < 0) {
        swap(i, StoreIndex);
        ++StoreIndex;
      }
    }
    
    // Move pivot to its final place.
    swap(StoreIndex, Right);
    
    // Return final index of pivot.
    return StoreIndex;
  }
  
  /// \brief Quicksort.
  ///
  /// \return true iff sorting should continue (no errors occurred).
  ///
  bool quicksort(std::size_t Left, std::size_t Right)
  {
    if (Left >= Right)
      return true;
    
    // Divide using naive "halfway" partition.
    auto const MaybePivotIndex = partition(Left,
                                           Right,
                                           Left + ((Right - Left) / 2));
    
    if (!MaybePivotIndex.assigned())
      return false;
    
    auto const PivotIndex = MaybePivotIndex.get<std::size_t>();
    
    // Recursively sort the left side (if there is one).
    if (PivotIndex != 0 && !quicksort(Left, PivotIndex - 1))
      return false;
    
    // Sort the right area.
    return quicksort(PivotIndex + 1, Right);
  }

public:
  /// \brief Constructor.
  ///
  QuickSortImpl(seec::trace::ThreadEnvironment &WithThread,
                char * const ForArray,
                std::size_t const WithElementCount,
                std::size_t const WithElementSize,
                int (* const WithCompare)(char const *, char const *))
  : ThreadListener(WithThread.getThreadListener()),
    Checker(WithThread.getThreadListener(),
            WithThread.getInstructionIndex(),
            seec::runtime_errors::format_selects::CStdFunction::qsort),
    Array(ForArray),
    ElementCount(WithElementCount),
    ElementSize(WithElementSize),
    Compare(WithCompare)
  {}
  
  /// \brief Perform the quicksort.
  ///
  void operator()()
  {
    acquireMemory();
    quicksort(0, ElementCount - 1);
    releaseMemory();
  }
};


} // namespace seec


extern "C" {


//===----------------------------------------------------------------------===//
// abort
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(abort)
()
{
  seec::stopThreadsAndWriteTrace();
  std::abort();
}


//===----------------------------------------------------------------------===//
// exit
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(exit)
(int exit_code)
{
  // Call intercepted atexit() registered functions.
  {
    std::lock_guard<std::mutex> Lock (seec::AtExitFunctionsMutex);
    
    while (!seec::AtExitFunctions.empty()) {
      auto Fn = seec::AtExitFunctions.top();
      seec::AtExitFunctions.pop();
      (*Fn)();
    }
  }
  
  seec::stopThreadsAndWriteTrace();
  std::exit(exit_code);
}


//===----------------------------------------------------------------------===//
// quick_exit
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(quick_exit)
(int exit_code)
{
  // Call intercepted at_quick_exit() registered functions.
  {
    std::lock_guard<std::mutex> Lock (seec::AtQuickExitFunctionsMutex);
    
    while (!seec::AtQuickExitFunctions.empty()) {
      auto Fn = seec::AtQuickExitFunctions.top();
      seec::AtQuickExitFunctions.pop();
      (*Fn)();
    }
  }
  
  seec::stopThreadsAndWriteTrace();
  std::_Exit(exit_code);
}


//===----------------------------------------------------------------------===//
// _Exit
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(_Exit)
(int exit_code)
{
  seec::stopThreadsAndWriteTrace();
  std::_Exit(exit_code);
}


//===----------------------------------------------------------------------===//
// atexit
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(atexit)
(void (*func)())
{
  std::lock_guard<std::mutex> Lock (seec::AtExitFunctionsMutex);
  
  seec::AtExitFunctions.push(func);
  
  return 0;
}


//===----------------------------------------------------------------------===//
// at_quick_exit
//===----------------------------------------------------------------------===//

int
SEEC_MANGLE_FUNCTION(at_quick_exit)
(void (*func)())
{
  std::lock_guard<std::mutex> Lock (seec::AtQuickExitFunctionsMutex);
  
  seec::AtQuickExitFunctions.push(func);
  
  return 0;
}


//===----------------------------------------------------------------------===//
// qsort
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(qsort)
(char *Array,
 std::size_t ElementCount,
 std::size_t ElementSize,
 int (*Compare)(char const *, char const *)
 )
{
  seec::QuickSortImpl(seec::trace::getThreadEnvironment(),
                      Array, ElementCount, ElementSize, Compare)();
}


//===----------------------------------------------------------------------===//
// bsearch
//===----------------------------------------------------------------------===//

void
SEEC_MANGLE_FUNCTION(bsearch)
(char const *Key,
 char const *Array,
 std::size_t ElementCount,
 std::size_t ElementSize,
 int (*Compare)(char const *, char const *)
 )
{
  seec::BinarySearchImpl(seec::trace::getThreadEnvironment(),
                         Key, Array, ElementCount, ElementSize, Compare)();
}


} // extern "C"
