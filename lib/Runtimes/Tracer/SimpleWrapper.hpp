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
#include "seec/Util/FixedWidthIntTypes.hpp"
#include "seec/Util/FunctionTraits.hpp"
#include "seec/Util/ScopeExit.hpp"
#include "seec/Util/TemplateSequence.hpp"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include <cerrno>
#include <climits>
#include <cstring>


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
    // Convert to an unsigned, fixed-width integer type, because that is what
    // the notifyValue overloads expect, and if we leave it up to the compiler
    // the conversion will be ambiguous.
    constexpr auto ByteWidth = sizeof(typename std::make_unsigned<T>::type);
    constexpr auto BitWidth = CHAR_BIT * ByteWidth;
    
    Listener.notifyValue(InstructionIndex,
                         Instruction,
                         typename seec::GetUInt<BitWidth>::type(Value));
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


//===----------------------------------------------------------------------===//
// WrappedInputPointer
//===----------------------------------------------------------------------===//

template<typename T>
class WrappedInputPointer {
  T Value;
  
  std::size_t Size;
  
public:
  WrappedInputPointer(T ForValue)
  : Value(ForValue),
    Size(sizeof(*ForValue))
  {}
  
  /// \name Flags.
  /// @{
  
  WrappedInputPointer &setSize(std::size_t Value) {
    Size = Value;
    return *this;
  }
  
  std::size_t getSize() const { return Size; }
  
  /// @} (Flags.)
  
  operator T() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return sizeof(*Value); }
};

template<>
class WrappedInputPointer<void const *> {
  void const *Value;
  
  std::size_t Size;
  
public:
  WrappedInputPointer(void const * ForValue)
  : Value(ForValue),
    Size(0)
  {}
  
  /// \name Flags.
  /// @{
  
  WrappedInputPointer &setSize(std::size_t Value) {
    Size = Value;
    return *this;
  }
  
  std::size_t getSize() const { return Size; }
  
  /// @} (Flags.)
  
  operator void const *() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return 0; }
};

template<typename T>
WrappedInputPointer<T> wrapInputPointer(T ForValue) {
  return WrappedInputPointer<T>(ForValue);
}

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
              Value.getSize(),
              seec::runtime_errors::format_selects::MemoryAccess::Read);
  }
};


//===----------------------------------------------------------------------===//
// WrappedInputCString
//===----------------------------------------------------------------------===//

class WrappedInputCString {
  char const *Value;
  
  bool IgnoreNull;
  
public:
  WrappedInputCString(char const *ForValue)
  : Value(ForValue),
    IgnoreNull(false)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedInputCString &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator char const *() const { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  /// @}
};

inline WrappedInputCString wrapInputCString(char const *ForValue) {
  return WrappedInputCString(ForValue);
}

/// \brief WrappedArgumentChecker specialization for WrappedInputCString.
///
template<>
class WrappedArgumentChecker<WrappedInputCString>
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
  bool check(WrappedInputCString &Value, int Parameter) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    return Checker.checkCStringRead(Parameter, Value);
  }
};


//===----------------------------------------------------------------------===//
// WrappedInputCStringArray
//===----------------------------------------------------------------------===//

class WrappedInputCStringArray {
  char * const *Value;
  
  bool IgnoreNull;
  
public:
  WrappedInputCStringArray(char * const *ForValue)
  : Value(ForValue),
    IgnoreNull(false)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedInputCStringArray &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator char * const *() const { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  /// @}
};

inline WrappedInputCStringArray wrapInputCStringArray(char * const *ForValue) {
  return WrappedInputCStringArray(ForValue);
}

/// \brief WrappedArgumentChecker specialization for WrappedInputCStringArray.
///
template<>
class WrappedArgumentChecker<WrappedInputCStringArray>
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
  bool check(WrappedInputCStringArray &Value, int Parameter) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    return Checker.checkCStringArray(Parameter, Value) > 0;
  }
};


//===----------------------------------------------------------------------===//
// WrappedInputFILE
//===----------------------------------------------------------------------===//

class WrappedInputFILE {
  FILE *Value;
  
  bool IgnoreNull;
  
public:
  WrappedInputFILE(FILE *ForValue)
  : Value(ForValue),
    IgnoreNull(false)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedInputFILE &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator FILE *() const { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  /// @}
};

inline WrappedInputFILE wrapInputFILE(FILE *ForValue) {
  return WrappedInputFILE(ForValue);
}

/// \brief WrappedArgumentChecker specialization for WrappedInputFILE.
///
template<>
class WrappedArgumentChecker<WrappedInputFILE>
{
  /// The underlying memory checker.
  seec::trace::CIOChecker &Checker;

public:
  /// \brief Construct a new WrappedArgumentChecker.
  ///
  WrappedArgumentChecker(seec::trace::CIOChecker &WithChecker)
  : Checker(WithChecker)
  {}
  
  /// \brief Check if the given value is OK.
  ///
  bool check(WrappedInputFILE &Value, int Parameter) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    return Checker.checkStreamIsValid(Parameter, Value);
  }
};


//===----------------------------------------------------------------------===//
// WrappedOutputPointer
//===----------------------------------------------------------------------===//

template<typename T>
class WrappedOutputPointer {
  T Value;
  
  std::size_t Size;
  
  bool IgnoreNull;
  
public:
  WrappedOutputPointer(T ForValue)
  : Value(ForValue),
    Size(sizeof(*ForValue)),
    IgnoreNull(false)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedOutputPointer &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  WrappedOutputPointer &setSize(std::size_t Value) {
    Size = Value;
    return *this;
  }
  
  std::size_t getSize() const { return Size; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator T() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return sizeof(*Value); }
  
  /// @} (Value information)
};

template<>
class WrappedOutputPointer<void *> {
  void *Value;
  
  std::size_t Size;
  
  bool IgnoreNull;
  
public:
  WrappedOutputPointer(void *ForValue)
  : Value(ForValue),
    Size(0),
    IgnoreNull(false)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedOutputPointer &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  WrappedOutputPointer &setSize(std::size_t Value) {
    Size = Value;
    return *this;
  }
  
  std::size_t getSize() const { return Size; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator void *() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return 0; }
  
  /// @} (Value information)
};

template<typename T>
WrappedOutputPointer<T> wrapOutputPointer(T ForValue) {
  return WrappedOutputPointer<T>(ForValue);
}

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
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    return Checker.checkMemoryExistsAndAccessibleForParameter(
              Parameter,
              Value.address(),
              Value.getSize(),
              seec::runtime_errors::format_selects::MemoryAccess::Write);
  }
};

/// \brief WrappedArgumentRecorder specialization for WrappedOutputPointer.
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
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    if (Success) {
      auto const Ptr = reinterpret_cast<char const *>(Value.address());
      Listener.recordUntypedState(Ptr, Value.getSize());
    }
    
    return true;
  }
};


//===----------------------------------------------------------------------===//
// WrappedOutputCString
//===----------------------------------------------------------------------===//

class WrappedOutputCString {
  char *Value;
  
  bool IgnoreNull;
  
  std::size_t MaximumSize;
  
public:
  WrappedOutputCString(char *ForValue)
  : Value(ForValue),
    IgnoreNull(false),
    MaximumSize(std::numeric_limits<std::size_t>::max())
  {}
  
  /// \name Flags
  /// @{
  
  WrappedOutputCString &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }
  
  WrappedOutputCString &setMaximumSize(std::size_t Value) {
    MaximumSize = Value;
    return *this;
  }
  
  std::size_t getMaximumSize() const { return MaximumSize; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator char *() const { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  /// @} (Value information)
};

inline WrappedOutputCString wrapOutputCString(char *ForValue) {
  return WrappedOutputCString(ForValue);
}

/// \brief WrappedArgumentChecker specialization for WrappedOutputCString.
///
template<>
class WrappedArgumentChecker<WrappedOutputCString>
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
  bool check(WrappedOutputCString const &Value, int Parameter) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    return Checker.checkMemoryExistsAndAccessibleForParameter(
              Parameter,
              Value.address(),
              Value.getMaximumSize(),
              seec::runtime_errors::format_selects::MemoryAccess::Write);
  }
};

/// \brief WrappedArgumentRecorder specialization for WrappedOutputCString.
///
template<>
class WrappedArgumentRecorder<WrappedOutputCString> {
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
  bool record(WrappedOutputCString const &Value, bool Success) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    if (Success) {
      auto const Length = std::strlen(Value) + 1;
      Listener.recordUntypedState(Value, Length);
    }
    
    return true;
  }
};


//===----------------------------------------------------------------------===//
// ResultStateRecorder
//===----------------------------------------------------------------------===//

class ResultStateRecorderForNoOp {
public:
  ResultStateRecorderForNoOp() {}
  
  template<typename T>
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener,
              T &&Value)
  {}
};

class ResultStateRecorderForStaticInternalCString {
  MemoryPermission Access;
  
public:
  ResultStateRecorderForStaticInternalCString(MemoryPermission WithAccess)
  : Access(WithAccess)
  {}
  
  template<typename T>
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener,
              T &&Value)
  {
    if (Value == nullptr)
      return;
    
    auto const Address = reinterpret_cast<uintptr_t>(Value);
    auto const Ptr = reinterpret_cast<char const *>(Value);
    auto const Length = std::strlen(Ptr) + 1;
    
    // Remove existing knowledge of the area.
    ThreadListener.removeKnownMemoryRegion(Address);

    // Set knowledge of the new string area.
    ThreadListener.addKnownMemoryRegion(Address, Length, Access);
    
    // Update memory state.
    ThreadListener.recordUntypedState(Ptr, Length);
  }
};

class ResultStateRecorderForStaticInternalObject {
  MemoryPermission Access;
  
public:
  ResultStateRecorderForStaticInternalObject(MemoryPermission WithAccess)
  : Access(WithAccess)
  {}
  
  template<typename T>
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener,
              T &&Value)
  {
    if (Value == nullptr)
      return;
    
    auto const Address = reinterpret_cast<uintptr_t>(Value);
    auto const Ptr = reinterpret_cast<char const *>(Value);
    auto const Length = sizeof(*Value);
    
    // Remove existing knowledge of the area.
    ThreadListener.removeKnownMemoryRegion(Address);
    
    // Set knowledge of the new string area.
    ThreadListener.addKnownMemoryRegion(Address, Length, Access);
    
    // Update memory state.
    ThreadListener.recordUntypedState(Ptr, Length);
  }
};


//===----------------------------------------------------------------------===//
// GlobalVariableTracker
//===----------------------------------------------------------------------===//

/// \brief Used to record if a wrapped function modified a global variable.
///
class GlobalVariableTracker {
  /// Pointer to the tracked global.
  char const * const Global;
  
  /// The size of the tracked global.
  std::size_t const Size;
  
  /// Holds the pre-call contents of the global.
  llvm::SmallVector<char, 16> PreState;
  
public:
  /// \brief Constructor.
  ///
  template<typename T>
  GlobalVariableTracker(T const &ForGlobal)
  : Global(reinterpret_cast<char const *>(&ForGlobal)),
    Size(sizeof(ForGlobal)),
    PreState()
  {}
  
  /// \brief Save the state of the global so that we can check if it changed.
  ///
  void savePreCallState()
  {
    PreState.resize(Size);
    std::memcpy(PreState.data(), Global, Size);
  }
  
  /// \brief Record the state of the global if it has changed.
  ///
  void recordChanges(seec::trace::TraceThreadListener &ThreadListener) const
  {
    // Update memory state if it has changed.
    if (std::memcmp(PreState.data(), Global, Size)) {
      ThreadListener.recordUntypedState(Global, Size);
    }
  }
};


//===----------------------------------------------------------------------===//
// SimpleWrapper
//===----------------------------------------------------------------------===//

template<typename RetT,
         typename FnT,
         typename SuccessPredT,
         typename ResultStateRecorderT,
         SimpleWrapperSetting... Settings>
class SimpleWrapperImpl
{
  /// The function that is wrapped.
  seec::runtime_errors::format_selects::CStdFunction FSFunction;
  
  /// \brief Check if the given setting is enabled for this wrapper.
  ///
  template<SimpleWrapperSetting Setting>
  constexpr bool isEnabled() {
    return isSettingInList<Setting, Settings...>();
  }
  
public:
  /// \brief Construct a new wrapper implementation for the given function.
  ///
  SimpleWrapperImpl(seec::runtime_errors::format_selects::CStdFunction ForFn)
  : FSFunction(ForFn)
  {}
  
  /// \brief Implementation of the wrapped function.
  ///
  template<int... ArgIs, typename... ArgTs>
  RetT impl(FnT &&Function,
            SuccessPredT &&SuccessPred,
            ResultStateRecorderT &&ResultStateRecorder,
            llvm::SmallVectorImpl<GlobalVariableTracker> &GVTrackers,
            seec::ct::sequence_int<ArgIs...>,
            ArgTs &&... Args)
  {
    auto &ProcessEnv = seec::trace::getProcessEnvironment();
    auto &ProcessListener = ProcessEnv.getProcessListener();
    
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
    
    // TODO: Don't acquire stream lock if we don't need a CIOChecker.
    auto StreamsAccessor = Listener.getProcessListener().getStreamsAccessor();
    
    // Create the memory checker.
    seec::trace::CIOChecker Checker {Listener,
                                     InstructionIndex,
                                     FSFunction,
                                     StreamsAccessor.getObject()};
    
    // Check each of the inputs.
    std::vector<bool> InputChecks {
      (WrappedArgumentChecker<typename std::remove_reference<ArgTs>::type>
                             (Checker).check(Args, ArgIs))...
    };
    
#ifndef NDEBUG
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
#endif
    
    // Get the pre-call value of errno.
    auto const PreCallErrno = errno;
    
    // Get the pre-call values of all global variables we are tracking.
    for (auto &GVTracker : GVTrackers)
      GVTracker.savePreCallState();
    
    // Call the original function.
    auto const Result = Function(std::forward<ArgTs>(Args)...);
    bool const Success = SuccessPred(Result);
    
    // Silence warnings about unused Success.
    (void)Success;
    
    // Notify the TraceThreadListener of the new value.
    typedef typename std::remove_reference<FnT>::type FnTLessReference;
    ListenerNotifier<RetT> Notifier;
    Notifier(Listener, InstructionIndex, Instruction, Result);
    
    // Record any changes to errno.
    if (errno != PreCallErrno) {
      Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                                  sizeof(errno));
    }
    
    // Record any changes to global variables we are tracking.
    for (auto const &GVTracker : GVTrackers)
      GVTracker.recordChanges(Listener);
    
    // Record state changes revealed by the return value.
    ResultStateRecorder.record(ProcessListener, Listener, Result);
    
    // Record each of the outputs.
    std::vector<bool> OutputRecords {
      (WrappedArgumentRecorder<typename std::remove_reference<ArgTs>::type>
                              (Listener).record(Args, Success))...
    };
    
#ifndef NDEBUG
    for (auto const OutputRecord : OutputRecords) {
      assert(OutputRecord && "Output record failed.");
    }
#endif
    
    // Do Listener's notification exit.
    Listener.exitPostNotification();
    
    return Result;
  }
};

template<typename FnT,
         typename SuccessPredT,
         typename ResultStateRecorderT,
         SimpleWrapperSetting... Settings>
class SimpleWrapperImpl<void,
                        FnT,
                        SuccessPredT,
                        ResultStateRecorderT,
                        Settings...>
{
  /// The function that is wrapped.
  seec::runtime_errors::format_selects::CStdFunction FSFunction;
  
  /// \brief Check if the given setting is enabled for this wrapper.
  ///
  template<SimpleWrapperSetting Setting>
  constexpr bool isEnabled() {
    return isSettingInList<Setting, Settings...>();
  }
  
public:
  /// \brief Construct a new wrapper implementation for the given function.
  ///
  SimpleWrapperImpl(seec::runtime_errors::format_selects::CStdFunction ForFn)
  : FSFunction(ForFn)
  {}
  
  /// \brief Implementation of the wrapped function.
  ///
  template<int... ArgIs, typename... ArgTs>
  void impl(FnT &&Function,
            SuccessPredT &&SuccessPred,
            ResultStateRecorderT &&ResultStateRecorder,
            llvm::SmallVectorImpl<GlobalVariableTracker> &GVTrackers,
            seec::ct::sequence_int<ArgIs...>,
            ArgTs &&... Args)
  {
    auto &ThreadEnv = seec::trace::getThreadEnvironment();
    auto &Listener = ThreadEnv.getThreadListener();
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
    
    // TODO: Don't acquire stream lock if we don't need a CIOChecker.
    auto StreamsAccessor = Listener.getProcessListener().getStreamsAccessor();
    
    // Create the memory checker.
    seec::trace::CIOChecker Checker {Listener,
                                     InstructionIndex,
                                     FSFunction,
                                     StreamsAccessor.getObject()};
    
    // Check each of the inputs.
    std::vector<bool> InputChecks {
      (WrappedArgumentChecker<typename std::remove_reference<ArgTs>::type>
                             (Checker).check(Args, ArgIs))...
    };
    
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
    
    // Get the pre-call value of errno.
    auto const PreCallErrno = errno;
    
    // Get the pre-call values of all global variables we are tracking.
    for (auto &GVTracker : GVTrackers)
      GVTracker.savePreCallState();
    
    // Call the original function.
    Function(std::forward<ArgTs>(Args)...);
    bool const Success = SuccessPred();
        
    // Record any changes to errno.
    if (errno != PreCallErrno) {
      Listener.recordUntypedState(reinterpret_cast<char const *>(&errno),
                                  sizeof(errno));
    }
    
    // Record any changes to global variables we are tracking.
    for (auto const &GVTracker : GVTrackers)
      GVTracker.recordChanges(Listener);
    
    // Record each of the outputs.
    std::vector<bool> OutputRecords {
      (WrappedArgumentRecorder<typename std::remove_reference<ArgTs>::type>
                              (Listener).record(Args, Success))...
    };
    
    for (auto const OutputRecord : OutputRecords) {
      assert(OutputRecord && "Output record failed.");
    }
    
    // Do Listener's notification exit.
    Listener.exitPostNotification();
  }
};

template<SimpleWrapperSetting... Settings>
class SimpleWrapper {
  /// \name Members
  /// @{
  
  /// The function that is wrapped.
  seec::runtime_errors::format_selects::CStdFunction FSFunction;
  
  /// Global variable trackers.
  llvm::SmallVector<GlobalVariableTracker, 4> GVTrackers;
  
  /// @}
  
public:
  /// \brief Construct a new wrapper.
  ///
  SimpleWrapper(seec::runtime_errors::format_selects::CStdFunction ForFunction)
  : FSFunction(ForFunction),
    GVTrackers()
  {}
  
  /// \brief Add a global variable tracker.
  ///
  template<typename T>
  SimpleWrapper &trackGlobal(T const &Global)
  {
    GVTrackers.push_back(GlobalVariableTracker{Global});
    return *this;
  }
  
  /// \brief Execute the wrapped function.
  ///
  template<typename FnT,
           typename SuccessPredT,
           typename ResultStateRecorderT,
           typename... ArgT>
  typename seec::FunctionTraits<typename std::remove_reference<FnT>::type
                                >::ReturnType
  operator()(FnT &&Function,
             SuccessPredT &&SuccessPred,
             ResultStateRecorderT &&ResultStateRecorder,
             ArgT &&... Args)
  {
    // Get the return type.
    typedef
      typename seec::FunctionTraits<typename std::remove_reference<FnT>::type
                                    >::ReturnType
      RetT;
    
    // Create the argument indices.
    typename seec::ct::generate_sequence_int<0, sizeof...(ArgT)>::type Indices;
    
    // Call the implementation struct.
    return SimpleWrapperImpl<RetT,
                             FnT,
                             SuccessPredT,
                             ResultStateRecorderT,
                             Settings...>
                             { FSFunction }
            .impl(std::forward<FnT>(Function),
                  std::forward<SuccessPredT>(SuccessPred),
                  std::forward<ResultStateRecorderT>(ResultStateRecorder),
                  GVTrackers,
                  Indices,
                  std::forward<ArgT>(Args)...);
  }
};


} // namespace seec
