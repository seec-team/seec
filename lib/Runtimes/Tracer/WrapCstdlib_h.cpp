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
#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/IR/Function.h"
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
    
    for (auto const ThreadListenerPtr : ProcessListener.getThreadListeners()) {
      ThreadListenerPtr->traceWrite();
      ThreadListenerPtr->traceFlush();
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

  /// LLVM representation of comparison function.
  llvm::Function const * const CompareFn;
  
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
      
      // Array element is always the left side of comparison, key object is
      // always the right side of comparison. Our pointer object information
      // in the shim is set according to this, so do not change.
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
    Compare(WithCompare),
    CompareFn(ThreadListener.getProcessListener()
                            .getFunctionAt(reinterpret_cast<uintptr_t>
                                                           (Compare)))
  {
    if (auto const ActiveFn = ThreadListener.getActiveFunction())
      ActiveFn->setActiveInstruction(WithThread.getInstruction());
  }
  
  /// \brief Perform the quicksort.
  ///
  void *operator()()
  {
    // TODO: This should be raised as a run-time error.
    assert(CompareFn && "Comparison function is unknown!");

    auto const Caller = ThreadListener.getActiveFunction();
    assert(Caller && !Caller->isShim());

    auto const Call = llvm::ImmutableCallSite(Caller->getActiveInstruction());
    auto const KeyPtrObj   = Caller->getPointerObject(Call.getArgument(0));
    auto const ArrayPtrObj = Caller->getPointerObject(Call.getArgument(1));

    ThreadListener.pushShimFunction();
    auto const Shim = ThreadListener.getActiveFunction();

    // Array element is always the left side of comparison, key object is
    // always the right side of comparison.
    auto CompareFnArg = CompareFn->arg_begin();
    Shim->setPointerObject(  &*CompareFnArg, ArrayPtrObj);
    Shim->setPointerObject(&*++CompareFnArg, KeyPtrObj  );

    acquireMemory();
    auto const Result = bsearch();
    releaseMemory();

    ThreadListener.popShimFunction();

    // The C standard specifies the result is not const.
    auto const Unqualified = const_cast<char *>(Result);

    // Notify of the returned pointer (and its pointer object).
    auto const CallInst = Call.getInstruction();
    auto const Idx = seec::trace::getThreadEnvironment().getInstructionIndex();

    ThreadListener.notifyValue(Idx, CallInst,
                               reinterpret_cast<void *>(Unqualified));

    // Note that Caller is invalidated when the shim function is pushed, so we
    // need to retrieve a new pointer to the active function.
    ThreadListener.getActiveFunction()
                  ->setPointerObject(CallInst,
                                     Result ? ArrayPtrObj
                                            : seec::trace::PointerTarget{});

    // const_cast due to the C standard.
    return Unqualified;
  }
};


/// \brief Implement a recording quicksort.
///
class QuickSortImpl {
  /// The process.
  seec::trace::TraceProcessListener &ProcessListener;

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

  /// LLVM representation of comparison function.
  llvm::Function const * const CompareFn;
  
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

    // Copy all in-memory pointer object tracking, in case the elements we are
    // moving are pointers (or contain pointers).
    auto const AddrOfA = reinterpret_cast<uintptr_t>(ElemA);
    auto const AddrOfB = reinterpret_cast<uintptr_t>(ElemB);
    auto const AddrOfT = reinterpret_cast<uintptr_t>(TempValue);
    ProcessListener.copyInMemoryPointerObjects(AddrOfA, AddrOfT, ElementSize);
    ProcessListener.copyInMemoryPointerObjects(AddrOfB, AddrOfA, ElementSize);
    ProcessListener.copyInMemoryPointerObjects(AddrOfT, AddrOfB, ElementSize);
    ProcessListener.clearInMemoryPointerObjects(MemoryArea(AddrOfT,
                                                           ElementSize));

    // Create a new thread time for this "step" of the sort, and record the
    // updated memory states.
    ThreadListener.incrementThreadTime();
    ThreadListener.recordUntypedState(ElemA, ElementSize);
    ThreadListener.recordUntypedState(ElemB, ElementSize);
  }
  
  /// \brief Partition.
  ///
  seec::Maybe<std::size_t>
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
        return seec::Maybe<std::size_t>();
            
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
  : ProcessListener(WithThread.getProcessEnvironment().getProcessListener()),
    ThreadListener(WithThread.getThreadListener()),
    Checker(WithThread.getThreadListener(),
            WithThread.getInstructionIndex(),
            seec::runtime_errors::format_selects::CStdFunction::qsort),
    Array(ForArray),
    ElementCount(WithElementCount),
    ElementSize(WithElementSize),
    Compare(WithCompare),
    CompareFn(ProcessListener.getFunctionAt(reinterpret_cast<uintptr_t>
                                                            (Compare)))
  {
    if (auto const ActiveFn = ThreadListener.getActiveFunction())
      ActiveFn->setActiveInstruction(WithThread.getInstruction());
  }
  
  /// \brief Perform the quicksort.
  ///
  void operator()()
  {
    // TODO: This should be raised as a run-time error.
    assert(CompareFn && "Comparison function is unknown!");

    auto const Caller = ThreadListener.getActiveFunction();
    assert(Caller && !Caller->isShim());

    auto const Call = llvm::ImmutableCallSite(Caller->getActiveInstruction());
    auto const ArrayPtrObj = Caller->getPointerObject(Call.getArgument(0));

    ThreadListener.pushShimFunction();
    auto const Shim = ThreadListener.getActiveFunction();

    auto CompareFnArg = CompareFn->arg_begin();
    Shim->setPointerObject(  &*CompareFnArg, ArrayPtrObj);
    Shim->setPointerObject(&*++CompareFnArg, ArrayPtrObj);

    acquireMemory();
    quicksort(0, ElementCount - 1);
    releaseMemory();

    ThreadListener.popShimFunction();
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

void *
SEEC_MANGLE_FUNCTION(bsearch)
(char const *Key,
 char const *Array,
 std::size_t ElementCount,
 std::size_t ElementSize,
 int (*Compare)(char const *, char const *)
 )
{
  return seec::BinarySearchImpl(seec::trace::getThreadEnvironment(),
                                Key, Array, ElementCount, ElementSize, Compare)
                               ();
}


} // extern "C"
