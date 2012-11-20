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

template<bool Pre, typename LT, llvm::Intrinsic::ID Intr>
struct DetectAndForwardIntrinsicImpl {
  static bool impl(LT &Lstn,
                   llvm::CallInst const *I,
                   uint32_t Index,
                   unsigned ID) {
    return false;
  }
};

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
