//===- seec/Trace/DetectCalls.hpp ----------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This file implements a method to detect whether a CallInst is calling a
// known function, and if it is, to get the live values of the arguments from
// the Listener (using getCurrentRuntimeValue), and to pass those arguments to a
// function-specific member function on the Listener. For example, if we were
// to detect the call ``malloc'' from the C Standard Library, the size argument
// would be extracted, and we would call the Listener's member function
// preCmalloc or postCmalloc, depending on whether we were detecting Pre or Post
// executing the CallInst.
//
// The functions that can be detected using this method are only those defined
// in the file DetectCallsAll.def (this file includes other lists of functions,
// e.g. DetectCallsCstdlib.def for C's <stdlib.h>).
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_DETECT_CALLS_HPP
#define SEEC_TRACE_DETECT_CALLS_HPP

#include "seec/Trace/DetectCallsLookup.hpp"
#include "seec/Trace/GetCurrentRuntimeValue.hpp"

#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"

#include <cstdio> // for fpos_t, size_t, FILE *.
#include <ctime> // for time_t, struct tm.

/// SeeC's root namespace.
namespace seec {

/// SeeC's trace-related functionality.
namespace trace {

/// Holds implementation details for detectCalls.
namespace detect_calls {

template<bool Pre, typename LT, Call C>
struct NotifyImpl {
  template<typename... PTs>
  static void impl(LT &Listener,
                   llvm::CallInst const *I,
                   uint32_t Index,
                   PTs&&... Params) {}
};

template<bool Pre, typename LT, Call C>
struct ExtractAndNotifyImpl {
  static bool impl(LT &Listener, llvm::CallInst const *I, uint32_t Index) {
    return false;
  }
};

/// Implements the detectCall functionality for a single known Call.
/// \tparam Pre Use PreCall notification. If true, the PreCall notification will
///             be used, otherwise the PostCall notification will be used.
/// \tparam LT The type of the listener object that will be notified if a Call
///            is detected.
/// \tparam C The type of call that this template instantiation will detect.
template<bool Pre, typename LT, Call C>
struct DetectCallImpl {
  /// Implementation of detectCall for Call C.
  /// \param Lk the Lookup used to find the run-time location of C.
  /// \param Listener the object that will be notified if the call matches.
  /// \param I the CallInst representing this call.
  /// \param Addr the run-time location being called to.
  /// \return true iff the call matched.
  static bool impl(Lookup const &Lk,
                   LT &Listener,
                   llvm::CallInst const *I,
                   uint32_t Index,
                   void const *Addr) {
    return false;
  }
};

template<bool Pre, typename LT, llvm::Intrinsic::ID Intr>
struct DetectAndForwardIntrinsicImpl {
  static bool impl(LT &Lstn,
                   llvm::CallInst const *I,
                   uint32_t Index,
                   unsigned ID) {
    return false;
  }
};

/// The base case for the recursion of detectCalls. No more calls to check, so
/// return false.
/// \tparam Pre Use PreCall notification.
/// \tparam LT The type of the listener object that will be notified if a Call
///            is detected.
/// \param Lk the Lookup used to find the run-time location of calls.
/// \param Listener the object that would be notified if a call matched.
/// \param I the CallInst representing this call.
/// \param Addr the run-time location being called to.
/// \return false.
template<bool Pre, typename LT>
bool detectCalls(Lookup const &Lk,
                 LT &Listener,
                 llvm::CallInst const *I,
                 uint32_t Index,
                 void const *Addr) {
  return false;
}

/// Detect a known Call and notify the PreCall or PostCall member function,
/// depending on the value of the template parameter Pre. This variadic
/// template takes a list of Calls to match against, e.g.:
/// detectCalls<true, LT, Call::Cmalloc, Call::Cfree>(...);
/// One can also use groups of Calls, e.g.:
/// detectCalls<true, LT, Call::Cstdlib_memory>(...);
/// \param Lk the Lookup used to find the run-time locations of Calls.
/// \param Listener the object that will be notified if the call matches.
/// \param I the CallInst representing this call.
/// \param Addr the run-time location being called to.
/// \return true iff the call was matched.
template<bool Pre, typename LT, Call C, Call... CS>
bool detectCalls(Lookup const &Lk,
                 LT &Listener,
                 llvm::CallInst const *I,
                 uint32_t Index,
                 void const *Addr) {
  // Try to detect the first Call from the variadic list of Calls.
  if (DetectCallImpl<Pre, LT, C>::impl(Lk, Listener, I, Index, Addr))
    return true;

  // Recursively try to detect all remaining Calls.
  return detectCalls<Pre, LT, CS...>(Lk, Listener, I, Index, Addr);
}

/// Detect a known Call and notify the PreCall member function. This variadic
/// template takes a list of Calls to match against, e.g.:
/// detectPreCalls<LT, Call::Cmalloc, Call::Cfree>(...);
/// One can also use groups of Calls, e.g.:
/// detectPreCalls<LT, Call::Cstdlib_memory>(...);
/// \param Lk the Lookup used to find the run-time locations of Calls.
/// \param Listener the object that will be notified if the call matches.
/// \param I the CallInst representing this call.
/// \param Addr the run-time location being called to.
/// \return true iff the call was matched.
template<typename LT, Call C, Call... CS>
bool detectPreCalls(Lookup const &Lk,
                    LT &Listener,
                    llvm::CallInst const *I,
                    uint32_t Index,
                    void const *Addr) {
  return detectCalls<true, LT, C, CS...>(Lk, Listener, I, Index, Addr);
}

/// Detect a known Call and notify the PostCall member function. This variadic
/// template takes a list of Calls to match against, e.g.:
/// detectPostCalls<LT, Call::Cmalloc, Call::Cfree>(...);
/// One can also use groups of Calls, e.g.:
/// detectPostCalls<LT, Call::Cstdlib_memory>(...);
/// \param Lk the Lookup used to find the run-time locations of Calls.
/// \param Listener the object that will be notified if the call matches.
/// \param I the CallInst representing this call.
/// \param Addr the run-time location being called to.
/// \return true iff the call was matched.
template<typename LT, Call C, Call... CS>
bool detectPostCalls(Lookup const &Lk,
                     LT &Listener,
                     llvm::CallInst const *I,
                     uint32_t Index,
                     void const *Addr) {
  return detectCalls<false, LT, C, CS...>(Lk, Listener, I, Index, Addr);
}

template<bool Pre, typename LT>
bool detectAndForwardIntrinsics(LT &Listener,
                                llvm::CallInst const *I,
                                uint32_t Index,
                                unsigned ID) {
  return false;
}

template<bool Pre,
         typename LT,
         llvm::Intrinsic::ID Intr,
         llvm::Intrinsic::ID... Intrs
         >
bool detectAndForwardIntrinsics(LT &Listener,
                                llvm::CallInst const *I,
                                uint32_t Index,
                                unsigned ID) {
  if (DetectAndForwardIntrinsicImpl<Pre, LT, Intr>::impl(Listener, 
                                                         I, 
                                                         Index, 
                                                         ID))
    return true;

  return detectAndForwardIntrinsics<Pre, LT, Intrs...>(Listener, I, Index, ID);
}

template<typename LT, llvm::Intrinsic::ID... Intrs>
bool detectAndForwardPreIntrinsics(LT &Listener,
                                   llvm::CallInst const *I,
                                   uint32_t Index,
                                   unsigned ID) {
  return detectAndForwardIntrinsics<true, LT, Intrs...>(Listener, I, Index, ID);
}

template<typename LT, llvm::Intrinsic::ID... Intrs>
bool detectAndForwardPostIntrinsics(LT &Listener,
                                    llvm::CallInst const *I,
                                    uint32_t Index,
                                    unsigned ID) {
  return detectAndForwardIntrinsics<false, LT, Intrs...>(Listener,
                                                         I,
                                                         Index,
                                                         ID);
}

// X-macro to generate specializations for the known calls
// generate call groups
#define DETECT_CALL_PREFIXIFY(PREFIX, CALL) Call::PREFIX ## CALL
#define DETECT_CALL_GROUP(PREFIX, GROUP, ...)                                  \
template<bool Pre, typename LT>                                                \
struct DetectCallImpl<Pre, LT, Call::PREFIX ## GROUP> {                        \
  static bool impl(Lookup const &Lk,                                           \
                   LT &L,                                                      \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   void const *Addr) {                                         \
    return detectCalls<Pre, LT, __VA_ARGS__>(Lk, L, I, Index, Addr);           \
  }                                                                            \
};

// generate pre/post call notification (NotifyImpl), argument lookup
// (ExtractAndNotifyImpl), and detection (DetectCallImpl).
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS)                                \
template<typename LT>                                                          \
struct NotifyImpl<true, LT, Call::PREFIX ## NAME> {                            \
  template<typename... PTs>                                                    \
  static void impl(LT &Listener,                                               \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   PTs&&... Params) {                                          \
    Listener.pre ## PREFIX ## NAME (I, Index, std::forward<PTs>(Params)...);   \
  }                                                                            \
};                                                                             \
template<typename LT>                                                          \
struct NotifyImpl<false, LT, Call::PREFIX ## NAME> {                           \
  template<typename... PTs>                                                    \
  static void impl(LT &Listener,                                               \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   PTs&&... Params) {                                          \
    Listener.post ## PREFIX ## NAME (I, Index, std::forward<PTs>(Params)...);  \
  }                                                                            \
};                                                                             \
template<bool Pre, typename LT>                                                \
struct ExtractAndNotifyImpl<Pre, LT, Call::PREFIX ## NAME> {                   \
  static bool impl(LT &Listener, llvm::CallInst const *I, uint32_t Index) {    \
    LOCALS                                                                     \
    if (!getArgumentValues(Listener, I, 0 ARGS))                               \
      return false;                                                            \
    NotifyImpl<Pre, LT, Call::PREFIX ## NAME>::impl(Listener, I, Index ARGS);  \
    return true;                                                               \
  }                                                                            \
};                                                                             \
template<bool Pre, typename LT>                                                \
struct DetectCallImpl<Pre, LT, Call::PREFIX ## NAME> {                         \
  static bool impl(Lookup const &Lk,                                           \
                   LT &Lstn,                                                   \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   void const *Addr) {                                         \
    if (!Lk.Check(Call::PREFIX ## NAME, Addr))                                 \
      return false;                                                            \
    return ExtractAndNotifyImpl<Pre, LT, Call::PREFIX##NAME>::impl(Lstn,       \
                                                                   I,          \
                                                                   Index);     \
  }                                                                            \
};

// generate detection for intrinsics
#define DETECT_CALL_FORWARD_INTRINSIC(INTRINSIC, PREFIX, CALL)                 \
template<bool Pre, typename LT>                                                \
struct DetectAndForwardIntrinsicImpl<Pre, LT, llvm::Intrinsic::ID::INTRINSIC> {\
  static bool impl(LT &Lstn,                                                   \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   unsigned ID) {                                              \
    if (llvm::Intrinsic::ID::INTRINSIC != ID)                                  \
      return false;                                                            \
    return ExtractAndNotifyImpl<Pre, LT, Call::PREFIX##CALL>::impl(Lstn,       \
                                                                   I,          \
                                                                   Index);     \
  }                                                                            \
};
/// \cond
#include "DetectCallsAll.def"
/// \endcond

} // namespace detect_calls


template<class SubclassT>
class CallDetector {
  seec::trace::detect_calls::Lookup const &CallLookup;
  
public:
  CallDetector(seec::trace::detect_calls::Lookup const &WithLookup)
  : CallLookup(WithLookup)
  {}
  
  bool detectPreCall(llvm::CallInst const *Instruction,
                     uint32_t Index,
                     void const *Address) {
    using namespace seec::trace::detect_calls;
    
    auto MaybeCall = CallLookup.Check(Address);
    if (!MaybeCall.assigned())
      return false;
    
    switch (MaybeCall.get<0>()) {
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS)                                \
      case Call::PREFIX##NAME:                                                 \
        return ExtractAndNotifyImpl<true, SubclassT, Call::PREFIX##NAME>       \
                  ::impl(*static_cast<SubclassT *>(this), Instruction, Index);
#define DETECT_CALL_GROUP(PREFIX, GROUP, ...)                                  \
      case Call::PREFIX##GROUP: return false;
#include "DetectCallsAll.def"
      
      case Call::highest:
        llvm_unreachable("Detected invalid Call.");
        return false;
    }
    
    return false;
  }
  
  bool detectPostCall(llvm::CallInst const *Instruction,
                      uint32_t Index,
                      void const *Address) {
    using namespace seec::trace::detect_calls;
    
    auto MaybeCall = CallLookup.Check(Address);
    if (!MaybeCall.assigned())
      return false;
    
    switch (MaybeCall.get<0>()) {
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS)                                \
      case Call::PREFIX##NAME:                                                 \
        return ExtractAndNotifyImpl<false, SubclassT, Call::PREFIX##NAME>      \
                  ::impl(*static_cast<SubclassT *>(this), Instruction, Index);
#define DETECT_CALL_GROUP(PREFIX, GROUP, ...)                                  \
      case Call::PREFIX##GROUP: return false;
#include "DetectCallsAll.def"
      
      case Call::highest:
        llvm_unreachable("Detected invalid Call.");
        return false;
    }
    
    return false;
  }
  
  // Define empty notification functions that will be called if SubclassT does
  // not implement a notification function.
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS)                                \
  template<typename... ArgTs>                                                  \
  void pre##PREFIX##NAME(llvm::CallInst const *Instruction, uint32_t Index,    \
                         ArgTs &&...Args) {}                                   \
  template<typename... ArgTs>                                                  \
  void post##PREFIX##NAME(llvm::CallInst const *Instruction, uint32_t Index,   \
                          ArgTs &&...Args) {}
/// \cond
#include "DetectCallsAll.def"
/// \endcond
};


} // namespace trace

} // namespace seec

#endif // SEEC_TRACE_DETECT_CALLS_HPP
