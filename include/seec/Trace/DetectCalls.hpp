//===- seec/Trace/DetectCalls.hpp ----------------------------------- C++ -===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements a method to detect whether a CallInst is calling a
/// known function, and if it is, to get the live values of the arguments from
/// the Listener (using getCurrentRuntimeValue), and to pass those arguments to
/// a function-specific member function on the Listener. For example, if we were
/// to detect the call ``malloc'' from the C Standard Library, the size argument
/// would be extracted, and we would call the Listener's member function
/// preCmalloc or postCmalloc, depending on whether we were detecting Pre or
/// Post executing the CallInst.
///
/// The functions that can be detected using this method are only those defined
/// in the file DetectCallsAll.def (this file includes other lists of functions,
/// e.g. DetectCallsCstdlib.def for C's <stdlib.h>).
///
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_DETECT_CALLS_HPP
#define SEEC_TRACE_DETECT_CALLS_HPP

#include "seec/Preprocessor/AddComma.h"
#include "seec/Trace/DetectCallsLookup.hpp"
#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/TemplateSequence.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/CallSite.h"

#include <cstdio> // for fpos_t, size_t, FILE *.
#include <ctime> // for time_t, struct tm.
#include <tuple>

/// SeeC's root namespace.
namespace seec {

/// SeeC's trace-related functionality.
namespace trace {

/// Holds implementation details for detectCalls.
namespace detect_calls {


//===------------------------------------------------------------------------===
// VarArgList
//===------------------------------------------------------------------------===

struct ExtractVarArgList {};

/// \brief Used to detect variadic arguments.
template<typename LT>
class VarArgList {
  LT &Listener;
  
  llvm::ImmutableCallSite Instruction;
  
  unsigned Offset;

public:
  VarArgList(LT &TheListener,
             llvm::ImmutableCallSite TheInstruction,
             unsigned TheOffset)
  : Listener(TheListener),
    Instruction(TheInstruction),
    Offset(TheOffset)
  {}
  
  unsigned size() const {
    return Instruction.arg_size() - Offset;
  }
  
  unsigned offset() const {
    return Offset;
  }
  
  template<typename T>
  seec::Maybe<T> getAs(unsigned Index) const {
    if (Offset + Index < Instruction.arg_size()) {
      auto Arg = Instruction.getArgument(Offset + Index);
      return getCurrentRuntimeValueAs<T>(Listener, Arg);
    }
    
    return seec::Maybe<T>();
  }
};


//===------------------------------------------------------------------------===
// getArgumentAs<T>
//===------------------------------------------------------------------------===

/// \brief Get the runtime value of an argument as a specified type.
template<typename T>
struct GetArgumentImpl {
  template<typename LT>
  static T impl(LT &Listener, llvm::CallInst const *Instr, std::size_t Arg)
  {
    auto Value = getCurrentRuntimeValueAs<T>(Listener,
                                             Instr->getArgOperand(Arg));
    return Value.template get<0>();
  }
};

template<typename LT>
struct GetArgumentImpl<VarArgList<LT>> {
  static VarArgList<LT> impl(LT &Listener,
                             llvm::CallInst const *Instr,
                             std::size_t Arg)
  {
    return VarArgList<LT>(Listener, llvm::ImmutableCallSite{Instr}, Arg);
  }
};

template<typename T, typename LT>
T getArgumentAs(LT &Listener, llvm::CallInst const *Instr, std::size_t Arg) {
  return GetArgumentImpl<T>::impl(Listener, Instr, Arg);
}


//===------------------------------------------------------------------------===
// NotifyImpl
//===------------------------------------------------------------------------===

/// This is specialized for each Call.
template<bool Pre, Call C>
struct NotifyImpl;


//===------------------------------------------------------------------------===
// ExtractAndNotify
//===------------------------------------------------------------------------===

/// \brief Extracts arguments and defers to NotifyImpl.
template<bool Pre, Call C, typename... ArgTs>
struct ExtractAndNotifyImpl {
  typedef std::tuple<ArgTs...> TupleTy;
  
  template<typename LT, int... ArgIs>
  static bool impl(LT &Listener,
                   llvm::CallInst const *I,
                   uint32_t Index,
                   seec::ct::sequence_int<ArgIs...>) {
    NotifyImpl<Pre, C>
      ::impl(Listener,
             I,
             Index,
             getArgumentAs<typename std::tuple_element<ArgIs, TupleTy>::type>
                          (Listener, I, ArgIs)...
            );
    
    return true;
  }
};

/// This generates a compile-time sequence of ints holding the indices of all
/// arguments to extract, and then defers to ExtractAndNotifyImpl.
template<bool Pre, Call C, typename... ArgTs>
struct ExtractAndNotifyForward {
  typedef typename seec::ct::generate_sequence_int<0, sizeof...(ArgTs)>::type
          SeqTy;
  
  template<typename LT>
  static bool impl(LT &Listener, llvm::CallInst const *Instr, uint32_t Index) {
    return ExtractAndNotifyImpl<Pre, C, ArgTs...>::impl(Listener,
                                                        Instr,
                                                        Index,
                                                        SeqTy());
  }
};

/// This is specialized for each Call, and simply defers to
/// ExtractAndNotifyForward with the appropriate argument types for the Call.
template<bool Pre, Call C>
struct ExtractAndNotify {
  template<typename LT>
  static bool impl(LT &Listener, llvm::CallInst const *I, uint32_t Index) {
    return false;
  }
};


//===------------------------------------------------------------------------===
// DetectAndForwardIntrinsicImpl
//===------------------------------------------------------------------------===

template<bool Pre, typename LT, llvm::Intrinsic::ID Intr>
struct DetectAndForwardIntrinsicImpl {
  static bool impl(LT &Lstn,
                   llvm::CallInst const *I,
                   uint32_t Index,
                   unsigned ID) {
    return false;
  }
};


//===------------------------------------------------------------------------===
// detectAndForward*Intrinsics
//===------------------------------------------------------------------------===

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


//===------------------------------------------------------------------------===
// Generate specializations of NotifyImpl, ExtractAndNotifyImpl, and
// DetectAndForwardIntrinsicImpl for all known calls.
//===------------------------------------------------------------------------===

// Generate pre/post call notification (NotifyImpl).
#define DETECT_CALL(PREFIX, NAME, ARGTYPES)                                    \
template<>                                                                     \
struct NotifyImpl<true, Call::PREFIX ## NAME> {                                \
  template<typename LT, typename... PTs>                                       \
  static void impl(LT &Listener,                                               \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   PTs&&... Params) {                                          \
    Listener.pre ## PREFIX ## NAME (I, Index, std::forward<PTs>(Params)...);   \
  }                                                                            \
};                                                                             \
template<>                                                                     \
struct NotifyImpl<false, Call::PREFIX ## NAME> {                               \
  template<typename LT, typename... PTs>                                       \
  static void impl(LT &Listener,                                               \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   PTs&&... Params) {                                          \
    Listener.post ## PREFIX ## NAME (I, Index, std::forward<PTs>(Params)...);  \
  }                                                                            \
};                                                                             \
template<bool Pre>                                                             \
struct ExtractAndNotify<Pre, Call::PREFIX##NAME> {                             \
  template<typename LT>                                                        \
  static bool impl(LT &Listener, llvm::CallInst const *I, uint32_t Index) {    \
    return ExtractAndNotifyForward                                             \
           <Pre, Call::PREFIX##NAME                                            \
            SEEC_PP_PREPEND_COMMA_IF_NOT_EMPTY(ARGTYPES)>                      \
           ::impl(Listener, I, Index);                                         \
  }                                                                            \
};

// Generate detection for intrinsics (DetectAndForwardIntrinsicImpl).
#define DETECT_CALL_FORWARD_INTRINSIC(INTRINSIC, PREFIX, CALL)                 \
template<bool Pre, typename LT>                                                \
struct DetectAndForwardIntrinsicImpl<Pre, LT, llvm::Intrinsic::ID::INTRINSIC> {\
  static bool impl(LT &Lstn,                                                   \
                   llvm::CallInst const *I,                                    \
                   uint32_t Index,                                             \
                   unsigned ID) {                                              \
    if (llvm::Intrinsic::ID::INTRINSIC != ID)                                  \
      return false;                                                            \
    return ExtractAndNotify<Pre, Call::PREFIX##CALL>::impl(Lstn, I, Index);    \
  }                                                                            \
};

/// \cond
#include "DetectCallsAll.def"
/// \endcond


} // namespace detect_calls


//===------------------------------------------------------------------------===
// CallDetector<LT>
//===------------------------------------------------------------------------===

template<class LT>
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
    
    switch (MaybeCall.template get<0>()) {
#define DETECT_CALL(PREFIX, NAME, ARGTYPES)                                    \
      case Call::PREFIX##NAME:                                                 \
        return ExtractAndNotify<true, Call::PREFIX##NAME>                      \
                  ::impl(*static_cast<LT *>(this), Instruction, Index);
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
    
    switch (MaybeCall.template get<0>()) {
#define DETECT_CALL(PREFIX, NAME, ARGTYPES)                                    \
      case Call::PREFIX##NAME:                                                 \
        return ExtractAndNotify<false, Call::PREFIX##NAME>                     \
                  ::impl(*static_cast<LT *>(this), Instruction, Index);
#include "DetectCallsAll.def"
      
      case Call::highest:
        llvm_unreachable("Detected invalid Call.");
        return false;
    }
    
    return false;
  }
  
  // Define empty notification functions that will be called if SubclassT does
  // not implement a notification function.
#define DETECT_CALL(PREFIX, NAME, ARGTYPES)                                    \
  void pre##PREFIX##NAME(llvm::CallInst const *Instruction, uint32_t Index     \
                         SEEC_PP_PREPEND_COMMA_IF_NOT_EMPTY(ARGTYPES)) {}      \
  void post##PREFIX##NAME(llvm::CallInst const *Instruction, uint32_t Index    \
                          SEEC_PP_PREPEND_COMMA_IF_NOT_EMPTY(ARGTYPES)) {}
/// \cond
#include "DetectCallsAll.def"
/// \endcond
};


} // namespace trace

} // namespace seec

#endif // SEEC_TRACE_DETECT_CALLS_HPP
