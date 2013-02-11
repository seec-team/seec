//===- lib/Runtimes/Tracer/WrapCfenv_h.cpp --------------------------------===//
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


#include "Tracer.hpp"

#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/FunctionTraits.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/Util/TemplateSequence.hpp"

#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"


// Forward declarations.
namespace llvm {
  class Instruction;
}


namespace seec {


//===----------------------------------------------------------------------===//
// ListenerNotifier
//===----------------------------------------------------------------------===//

template<typename T, typename Enable = void>
struct ListenerNotifier;

template<typename T>
struct ListenerNotifier
<T, typename std::enable_if<std::is_integral<T>::value>::type>
{
  void operator()(seec::trace::TraceThreadListener &Listener,
                  uint32_t InstructionIndex,
                  llvm::Instruction const *Instruction,
                  T Value) {
    Listener.notifyValue(InstructionIndex,
                         Instruction,
                         typename std::make_unsigned<T>::type(Value));
  }
};

template<typename T>
struct ListenerNotifier
<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
{
  void operator()(seec::trace::TraceThreadListener &Listener,
                  uint32_t InstructionIndex,
                  llvm::Instruction const *Instruction,
                  T Value) {
    Listener.notifyValue(InstructionIndex, Instruction, Value);
  }
};

template<typename T>
struct ListenerNotifier
<T, typename std::enable_if<std::is_pointer<T>::value>::type>
{
  void operator()(seec::trace::TraceThreadListener &Listener,
                  uint32_t InstructionIndex,
                  llvm::Instruction const *Instruction,
                  T Value) {
    Listener.notifyValue(InstructionIndex, Instruction, Value);
  }
};


//===----------------------------------------------------------------------===//
// SimpleWrapperSetting
//===----------------------------------------------------------------------===//

enum class SimpleWrapperSetting {
  AcquireGlobalMemoryReadLock,
  AcquireGlobalMemoryWriteLock,
  AcquireDynamicMemoryLock
};

template<SimpleWrapperSetting Value>
constexpr bool isSettingInList() {
  return false;
}

template<SimpleWrapperSetting Value,
         SimpleWrapperSetting Head,
         SimpleWrapperSetting... Tail>
constexpr bool isSettingInList() {
  return (Value == Head ? true : isSettingInList<Value, Tail...>());
}


//===----------------------------------------------------------------------===//
// WrappedInputPointer
//===----------------------------------------------------------------------===//

template<typename T>
class WrappedInputPointer {
  T Value;
  
public:
  WrappedInputPointer(T ForValue)
  : Value(ForValue)
  {}
  
  operator T() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return sizeof(*Value); }
};

template<typename T>
WrappedInputPointer<T> wrapInputPointer(T ForValue) {
  return WrappedInputPointer<T>(ForValue);
}


//===----------------------------------------------------------------------===//
// WrappedOutputPointer
//===----------------------------------------------------------------------===//

template<typename T>
class WrappedOutputPointer {
  T Value;
  
public:
  WrappedOutputPointer(T ForValue)
  : Value(ForValue)
  {}
  
  operator T() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return sizeof(*Value); }
};

template<typename T>
WrappedOutputPointer<T> wrapOutputPointer(T ForValue) {
  return WrappedOutputPointer<T>(ForValue);
}


//===----------------------------------------------------------------------===//
// WrappedArgumentChecker
//===----------------------------------------------------------------------===//

/// \brief Base case of WrappedArgumentChecker always passes checks.
///
template<typename T>
class WrappedArgumentChecker {
  /// The underlying memory checker.
  seec::trace::CStdLibChecker &Checker;

public:
  /// \brief Construct a new WrappedArgumentChecker.
  ///
  WrappedArgumentChecker(seec::trace::CStdLibChecker &WithChecker)
  : Checker(WithChecker)
  {}
  
  /// \brief Check if the given value is OK.
  ///
  bool check(T &Value, int Parameter) { return true; }
};


/// \brief WrappedArgumentChecker specialization for WrappedInputPointer.
///
template<typename T>
class WrappedArgumentChecker<WrappedInputPointer<T>>
{
  /// The underlying memory checker.
  seec::trace::CStdLibChecker &Checker;

public:
  /// \brief Construct a new WrappedArgumentChecker.
  ///
  WrappedArgumentChecker(seec::trace::CStdLibChecker &WithChecker)
  : Checker(WithChecker)
  {}
  
  /// \brief Check if the given value is OK.
  ///
  bool check(WrappedInputPointer<T> &Value, int Parameter) {
    return Checker.checkMemoryExistsAndAccessibleForParameter(
              Parameter,
              Value.address(),
              Value.pointeeSize(),
              seec::runtime_errors::format_selects::MemoryAccess::Read);
  }
};


/// \brief WrappedArgumentChecker specialization for WrappedOutputPointer.
///
template<typename T>
class WrappedArgumentChecker<WrappedOutputPointer<T>>
{
  /// The underlying memory checker.
  seec::trace::CStdLibChecker &Checker;

public:
  /// \brief Construct a new WrappedArgumentChecker.
  ///
  WrappedArgumentChecker(seec::trace::CStdLibChecker &WithChecker)
  : Checker(WithChecker)
  {}
  
  /// \brief Check if the given value is OK.
  ///
  bool check(WrappedOutputPointer<T> &Value, int Parameter) {
    return Checker.checkMemoryExistsAndAccessibleForParameter(
              Parameter,
              Value.address(),
              Value.pointeeSize(),
              seec::runtime_errors::format_selects::MemoryAccess::Write);
  }
};


//===----------------------------------------------------------------------===//
// WrappedArgumentRecorder
//===----------------------------------------------------------------------===//

/// \brief Base case of WrappedArgumentRecorder records nothing.
///
template<typename T>
class WrappedArgumentRecorder {
  /// The underlying TraceThreadListener.
  seec::trace::TraceThreadListener &Listener;

public:
  /// \brief Construct a new WrappedArgumentRecorder.
  ///
  WrappedArgumentRecorder(seec::trace::TraceThreadListener &WithListener)
  : Listener(WithListener)
  {}
  
  /// \brief Record any state changes.
  ///
  bool record(T &Value, bool Success) { return true; }
};


/// \brief Base case of WrappedArgumentRecorder records nothing.
///
template<typename T>
class WrappedArgumentRecorder<WrappedOutputPointer<T>> {
  /// The underlying TraceThreadListener.
  seec::trace::TraceThreadListener &Listener;

public:
  /// \brief Construct a new WrappedArgumentRecorder.
  ///
  WrappedArgumentRecorder(seec::trace::TraceThreadListener &WithListener)
  : Listener(WithListener)
  {}
  
  /// \brief Record any state changes.
  ///
  bool record(WrappedOutputPointer<T> &Value, bool Success) {
    if (Success) {
      auto const Ptr = reinterpret_cast<char const *>(Value.address());
      Listener.recordUntypedState(Ptr, Value.pointeeSize());
    }
    
    return true;
  }
};


//===----------------------------------------------------------------------===//
// SimpleWrapper
//===----------------------------------------------------------------------===//

template<SimpleWrapperSetting... Settings>
class SimpleWrapper {
  /// \name Members
  /// @{
  
  seec::runtime_errors::format_selects::CStdFunction FSFunction;
  
  /// @}
  
  
  /// \name Settings
  /// @{
  
  template<SimpleWrapperSetting Setting>
  constexpr bool isEnabled() {
    return isSettingInList<Setting, Settings...>();
  }
  
  /// @}
  
  template<typename FnT,
           typename SuccessPredT,
           int... ArgIs,
           typename... ArgT>
  typename seec::FunctionTraits<FnT>::ReturnType
  impl(FnT Function,
       SuccessPredT SuccessPred,
       seec::ct::sequence_int<ArgIs...>,
       ArgT... Args)
  {
    auto &ThreadEnv = seec::trace::getThreadEnvironment();
    auto &Listener = ThreadEnv.getThreadListener();
    auto Instruction = ThreadEnv.getInstruction();
    auto InstructionIndex = ThreadEnv.getInstructionIndex();
    
    // Do Listener's notification entry.
    Listener.enterNotification();
    
    // Acquire necessary locks.
    if (isEnabled<SimpleWrapperSetting::AcquireGlobalMemoryWriteLock>())
      Listener.acquireGlobalMemoryWriteLock();
    else if (isEnabled<SimpleWrapperSetting::AcquireGlobalMemoryReadLock>())
      Listener.acquireGlobalMemoryReadLock();
    
    if (isEnabled<SimpleWrapperSetting::AcquireDynamicMemoryLock>())
      Listener.acquireDynamicMemoryLock();
    
    // Create the memory checker.
    seec::trace::CStdLibChecker Checker{Listener, InstructionIndex, FSFunction};
    
    // Check each of the inputs.
    std::vector<bool> InputChecks {
      (WrappedArgumentChecker<ArgT>(Checker).check(Args, ArgIs))...
    };
    
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
    
    // Call the original function.
    auto const Result = Function(std::forward<ArgT>(Args)...);
    bool const Success = SuccessPred(Result);
    
    // Notify the TraceThreadListener of the new value.
    ListenerNotifier<typename seec::FunctionTraits<FnT>::ReturnType> Notifier;
    Notifier(Listener, InstructionIndex, Instruction, Result);
    
    // Record each of the outputs.
    std::vector<bool> OutputRecords {
      (WrappedArgumentRecorder<ArgT>(Listener).record(Args, Success))...
    };
    
    for (auto const OutputRecord : OutputRecords) {
      assert(OutputRecord && "Output record failed.");
    }
    
    // Do Listener's notification exit.
    Listener.exitPostNotification();
    
    return Result;
  }
  
public:
  SimpleWrapper(seec::runtime_errors::format_selects::CStdFunction ForFunction)
  : FSFunction(ForFunction)
  {}
  
  template<typename FnT,
           typename SuccessPredT,
           typename... ArgT>
  typename seec::FunctionTraits<FnT>::ReturnType
  operator()(FnT Function, SuccessPredT SuccessPred, ArgT... Args)
  {
    typename seec::ct::generate_sequence_int<0, sizeof...(ArgT)>::type Indices;
    return impl(std::forward<FnT>(Function),
                std::forward<SuccessPredT>(SuccessPred),
                Indices,
                std::forward<ArgT>(Args)...);
  }
};


} // namespace seec
