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
#include "llvm/IR/Instruction.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <type_traits>


// Forward declarations.
namespace llvm {
  class Instruction;
}


namespace seec {


//===----------------------------------------------------------------------===//
// recordErrno
//===----------------------------------------------------------------------===//

inline void recordErrno(seec::trace::TraceThreadListener &Thread,
                        int const &Errno)
{
  auto const CharPtr = reinterpret_cast<char const *>(&Errno);
  auto const Address = reinterpret_cast<uintptr_t>(CharPtr);
  auto const Length  = sizeof(Errno);

  if (!Thread.isKnownMemoryRegionCovering(Address, Length)) {
    // Set knowledge of the area.
    Thread.removeKnownMemoryRegion(Address);
    Thread.addKnownMemoryRegion(Address, Length, MemoryPermission::ReadWrite);
  }

  // Update memory state.
  Thread.recordUntypedState(CharPtr, Length);
}


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
// PointerOrigin
//===----------------------------------------------------------------------===//

/// \brief Possible sources of a returned or written pointer.
///
enum class PointerOrigin {
  None,
  FromArgument,
  NewValid
};


//===----------------------------------------------------------------------===//
// WrappedArgumentChecker
//===----------------------------------------------------------------------===//

/// \brief Base case of WrappedArgumentChecker always passes checks.
///
template<typename T>
class WrappedArgumentChecker {
public:
  /// \brief Construct a new WrappedArgumentChecker.
  ///
  WrappedArgumentChecker(seec::trace::CStdLibChecker &)
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
template<typename T, typename Enable = void>
class WrappedArgumentRecorder {
public:
  /// \brief Construct a new WrappedArgumentRecorder.
  ///
  WrappedArgumentRecorder(seec::trace::TraceProcessListener &,
                          seec::trace::TraceThreadListener &)
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
  
  bool IgnoreNull;

  bool ForCopy;
  
public:
  WrappedInputPointer(T ForValue)
  : Value(ForValue),
    Size(sizeof(*ForValue)),
    IgnoreNull(false),
    ForCopy(false)
  {}
  
  /// \name Flags.
  /// @{
  
  WrappedInputPointer &setSize(std::size_t Value) {
    Size = Value;
    return *this;
  }
  
  std::size_t getSize() const { return Size; }
  
  WrappedInputPointer &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }

  WrappedInputPointer &setForCopy(bool const Value) {
    ForCopy = Value;
    return *this;
  }

  bool getForCopy() const { return ForCopy; }
  
  /// @} (Flags.)
  
  operator T() { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  std::size_t pointeeSize() const { return sizeof(*Value); }
};

template<>
class WrappedInputPointer<void const *> {
  void const *Value;
  
  std::size_t Size;
  
  bool IgnoreNull;

  bool ForCopy;
  
public:
  WrappedInputPointer(void const * ForValue)
  : Value(ForValue),
    Size(0),
    IgnoreNull(false),
    ForCopy(false)
  {}
  
  /// \name Flags.
  /// @{
  
  WrappedInputPointer &setSize(std::size_t Value) {
    Size = Value;
    return *this;
  }
  
  std::size_t getSize() const { return Size; }
  
  WrappedInputPointer &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }

  WrappedInputPointer &setForCopy(bool const Value) {
    ForCopy = Value;
    return *this;
  }

  bool getForCopy() const { return ForCopy; }
  
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
    if (Value == nullptr && Value.getIgnoreNull())
      return true;
    
    auto const Access = Value.getForCopy()
      ? seec::runtime_errors::format_selects::MemoryAccess::Copy
      : seec::runtime_errors::format_selects::MemoryAccess::Read;

    return Checker.checkMemoryExistsAndAccessibleForParameter(
              Parameter,
              Value.address(),
              Value.getSize(),
              Access);
  }
};


//===----------------------------------------------------------------------===//
// WrappedInputCString
//===----------------------------------------------------------------------===//

template<typename CharT>
class WrappedInputCString {
  CharT *Value;

  bool IgnoreNull;

  bool IsLimited;

  std::size_t Limit;
  
public:
  WrappedInputCString(CharT *ForValue)
  : Value(ForValue),
    IgnoreNull(false),
    IsLimited(false),
    Limit(0)
  {}
  
  /// \name Flags
  /// @{
  
  WrappedInputCString &setIgnoreNull(bool Value) {
    IgnoreNull = Value;
    return *this;
  }
  
  bool getIgnoreNull() const { return IgnoreNull; }

  WrappedInputCString &setLimited(std::size_t const Value) {
    IsLimited = true;
    Limit = Value;
    return *this;
  }

  bool isLimited() const { return IsLimited; }

  std::size_t getLimit() const { return Limit; }
  
  /// @} (Flags)
  
  /// \name Value information
  /// @{
  
  operator CharT *() const { return Value; }
  
  uintptr_t address() const { return reinterpret_cast<uintptr_t>(Value); }
  
  /// @}
};

inline WrappedInputCString<char> wrapInputCString(char *ForValue) {
  return WrappedInputCString<char>(ForValue);
}

inline WrappedInputCString<char const> wrapInputCString(char const *ForValue) {
  return WrappedInputCString<char const>(ForValue);
}

/// \brief WrappedArgumentChecker specialization for WrappedInputCString.
///
template<typename CharT>
class WrappedArgumentChecker<WrappedInputCString<CharT>>
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
  bool check(WrappedInputCString<CharT> &Value, int Parameter) {
    if (Value == nullptr && Value.template getIgnoreNull())
      return true;
    
    if (Value.template isLimited()) {
      return Checker.checkLimitedCStringRead(Parameter, Value,
                                             Value.template getLimit());
    }
    else {
      return Checker.checkCStringRead(Parameter, Value);
    }
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

  PointerOrigin OutPtrOrigin;

  unsigned OutPtrOriginArg;

public:
  WrappedOutputPointer(T ForValue)
  : Value(ForValue),
    Size(sizeof(*ForValue)),
    IgnoreNull(false),
    OutPtrOrigin(PointerOrigin::None),
    OutPtrOriginArg(0)
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
  
  WrappedOutputPointer &setOriginNewValid() {
    OutPtrOrigin = PointerOrigin::NewValid;
    return *this;
  }

  WrappedOutputPointer &setOriginFromArg(unsigned const ArgNo) {
    OutPtrOrigin = PointerOrigin::FromArgument;
    OutPtrOriginArg = ArgNo;
  }

  PointerOrigin getOrigin() const { return OutPtrOrigin; }

  unsigned getOriginArg() const { return OutPtrOriginArg; }

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

  PointerOrigin getOrigin() const { return PointerOrigin::None; }

  unsigned getOriginArg() const { return 0; }

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

template<typename T>
struct is_pointer_to_pointer
: std::integral_constant
<bool,
 std::is_pointer<T>::value
 && std::is_pointer<typename std::remove_pointer<T>::type>::value>
{};

/// \brief WrappedArgumentRecorder specialization for WrappedOutputPointer
///        when the referenced object is also a pointer.
///
template<typename T>
class WrappedArgumentRecorder<WrappedOutputPointer<T>,
        typename std::enable_if<is_pointer_to_pointer<T>::value>::type>
{
  seec::trace::TraceProcessListener &Process;

  /// The underlying TraceThreadListener.
  seec::trace::TraceThreadListener &Listener;

public:
  /// \brief Construct a new WrappedArgumentRecorder.
  ///
  WrappedArgumentRecorder(seec::trace::TraceProcessListener &WithProcess,
                          seec::trace::TraceThreadListener &WithListener)
  : Process(WithProcess),
    Listener(WithListener)
  {}

  /// \brief Record any state changes.
  ///
  bool record(WrappedOutputPointer<T> &Value, bool Success) {
    if (Value == nullptr && Value.getIgnoreNull())
      return true;

    if (Success) {
      T const Ptr = Value;
      Listener.recordUntypedState(reinterpret_cast<char const *>(Ptr),
                                  Value.getSize());

      switch (Value.getOrigin()) {
        case PointerOrigin::None:
          llvm_unreachable("output pointer with no origin.");
          break;

        case PointerOrigin::FromArgument:
        {
          auto const Fn = Listener.getActiveFunction();
          auto const Inst = Fn->getActiveInstruction();
          auto const Call = llvm::dyn_cast<llvm::CallInst>(Inst);
          assert(Call && "active instruction is not a CallInst");

          auto const Arg = Call->getArgOperand(Value.getOriginArg());
          auto const Obj = Fn->getPointerObject(Arg);

          Process.setInMemoryPointerObject(reinterpret_cast<uintptr_t>(Ptr),
                                           Obj);
          break;
        }

        case PointerOrigin::NewValid:
          Process.setInMemoryPointerObject(
            reinterpret_cast<uintptr_t>(Ptr),
            Process.makePointerObject(reinterpret_cast<uintptr_t>(*Ptr)));
          break;
      }
    }

    return true;
  }
};

/// \brief WrappedArgumentRecorder specialization for WrappedOutputPointer.
///
template<typename T>
class WrappedArgumentRecorder<WrappedOutputPointer<T>,
        typename std::enable_if<!is_pointer_to_pointer<T>::value>::type>
{
  /// The underlying TraceThreadListener.
  seec::trace::TraceThreadListener &Listener;

public:
  /// \brief Construct a new WrappedArgumentRecorder.
  ///
  WrappedArgumentRecorder(seec::trace::TraceProcessListener &,
                          seec::trace::TraceThreadListener &WithListener)
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
  WrappedArgumentRecorder(seec::trace::TraceProcessListener &,
                          seec::trace::TraceThreadListener &WithListener)
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
  
  void record(seec::trace::TraceProcessListener &ProcessListener,
              seec::trace::TraceThreadListener &ThreadListener)
  {}
  
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
    
    if (!ThreadListener.isKnownMemoryRegionCovering(Address, Length)) {
      // Set knowledge of the area.
      ThreadListener.removeKnownMemoryRegion(Address);
      ThreadListener.addKnownMemoryRegion(Address, Length, Access);
    }
    
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

  /// Whether or not this global is a pointer.
  bool const IsPointerType;
  
public:
  /// \brief Constructor.
  ///
  template<typename T>
  GlobalVariableTracker(T const &ForGlobal)
  : Global(reinterpret_cast<char const *>(&ForGlobal)),
    Size(sizeof(ForGlobal)),
    PreState(),
    IsPointerType(std::is_pointer<T>::value)
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
    // Update memory state if it has changed, and the allocation is visible to
    // the user's program.
    if (!std::memcmp(PreState.data(), Global, Size))
      return;

    auto const Address = reinterpret_cast<uintptr_t>(Global);
    auto const MaybeArea = getContainingMemoryArea(ThreadListener, Address);
    if (!MaybeArea.assigned<MemoryArea>())
      return;

    auto const &Area = MaybeArea.get<MemoryArea>();
    if (!Area.contains(MemoryArea(Address, Size)))
      return;

    ThreadListener.recordUntypedState(Global, Size);

    if (IsPointerType) {
      auto &Process = ThreadListener.getProcessListener();
      auto const Value = *reinterpret_cast<uintptr_t const *>(Global);
      Process.setInMemoryPointerObject(Address,
                                       Process.makePointerObject(Value));
    }
  }
};


//===----------------------------------------------------------------------===//
// SimpleWrapper
//===----------------------------------------------------------------------===//

/// \brief Check if ArgT's WrappedArgumentChecker is constructible using the
///        primary checker CheckerT.
///
template<typename CheckerT, typename ArgT>
class RequireChecker
{
  typedef WrappedArgumentChecker<typename std::remove_reference<ArgT>::type>
          WACTy;

  static bool const C1 = std::is_constructible<WACTy, CheckerT &>::value;
  static bool const C2 =
    std::is_constructible<WACTy, CheckerT &, seec::trace::DIRChecker &>::value;

public:
  static bool const value = C1 || C2;
};

/// \brief Check if any of ArgTs's WrappedArgumentChecker types are
///        constructible using the primary checker CheckerT.
///
template<typename CheckerT, typename... ArgTs>
struct AnyRequireChecker
: seec::ct::static_any_of<RequireChecker<CheckerT, ArgTs>::value...> {};

/// \brief Check if the checker for ArgT requires a DIRChecker.
///
template<typename ArgT>
struct RequireDIRChecker
: std::is_constructible<
    WrappedArgumentChecker<typename std::remove_reference<ArgT>::type>,
    seec::trace::CIOChecker &,
    seec::trace::DIRChecker &> {};

/// \brief Check if any of the checkers for ArgTs requires a DIRChecker.
///
template<typename... ArgTs>
struct AnyRequireDIRChecker
: seec::ct::static_any_of<RequireDIRChecker<ArgTs>::value...> {};

/// \brief Dispatch to a single argument checker.
///
template<typename ArgT, typename Enable = void>
struct ArgumentCheckerDispatch;

template<typename ArgT>
struct ArgumentCheckerDispatch
  <ArgT, typename std::enable_if<RequireDIRChecker<ArgT>::value>::type>
{
  template<typename CheckT>
  static bool impl(CheckT &&Check,
                   seec::trace::DIRChecker *DIRCheck,
                   ArgT &Arg,
                   int Index)
  {
    return
      WrappedArgumentChecker<typename std::remove_reference<ArgT>::type>
                            (std::forward<CheckT>(Check), *DIRCheck)
                            .check(Arg, Index);
  }
};

template<typename ArgT>
struct ArgumentCheckerDispatch
  <ArgT, typename std::enable_if<!RequireDIRChecker<ArgT>::value>::type>
{
  template<typename CheckT>
  static bool impl(CheckT &&Check,
                   seec::trace::DIRChecker *DIRCheck,
                   ArgT &Arg,
                   int Index)
  {
    return
      WrappedArgumentChecker<typename std::remove_reference<ArgT>::type>
                            (std::forward<CheckT>(Check))
                            .check(Arg, Index);
  }
};

/// \brief Handles argument checking.
///
template<bool UseDIRChecker, bool UseCStdLib, bool UseCIO, typename...>
struct ArgumentCheckerHandlerImpl;

/// Specialization for no arguments.
///
template<>
struct ArgumentCheckerHandlerImpl<false, false, false, seec::ct::sequence_int<>>
{
  static void impl(seec::trace::TraceProcessListener &Process,
                   seec::trace::TraceThreadListener &Thread,
                   uint32_t const Instruction,
                   seec::runtime_errors::format_selects::CStdFunction const Fn)
  {}
};

/// Specialization for CIOChecker and DIRChecker.
///
template<bool UseCStdLib, int... ArgIs, typename... ArgTs>
struct ArgumentCheckerHandlerImpl
  <true, UseCStdLib, true, seec::ct::sequence_int<ArgIs...>, ArgTs...>
{
  static void impl(seec::trace::TraceProcessListener &Process,
                   seec::trace::TraceThreadListener &Thread,
                   uint32_t const Instruction,
                   seec::runtime_errors::format_selects::CStdFunction const Fn,
                   ArgTs &... Args)
  {
    auto StreamsAccessor = Process.getStreamsAccessor();
    seec::trace::CIOChecker Checker
      {Thread, Instruction, Fn, StreamsAccessor.getObject()};
    seec::trace::DIRChecker DIRChecker
      {Thread, Instruction, Fn, Thread.getDirs()};
    
    // Check each of the inputs.
    std::vector<bool> InputChecks {
      ArgumentCheckerDispatch<ArgTs>::impl(Checker, &DIRChecker, Args, ArgIs)...
    };
    
#ifndef NDEBUG
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
#endif
    
    InputChecks.clear();
  }
};

/// Specialization for CStdLibChecker and DIRChecker.
///
template<int... ArgIs, typename... ArgTs>
struct ArgumentCheckerHandlerImpl
  <true, true, false, seec::ct::sequence_int<ArgIs...>, ArgTs...>
{
  static void impl(seec::trace::TraceProcessListener &Process,
                   seec::trace::TraceThreadListener &Thread,
                   uint32_t const Instruction,
                   seec::runtime_errors::format_selects::CStdFunction const Fn,
                   ArgTs &... Args)
  {
    seec::trace::CStdLibChecker Checker{Thread, Instruction, Fn};
    seec::trace::DIRChecker DIRChecker
      {Thread, Instruction, Fn, Thread.getDirs()};
    
    // Check each of the inputs.
    std::vector<bool> InputChecks {
      ArgumentCheckerDispatch<ArgTs>::impl(Checker, &DIRChecker, Args, ArgIs)...
    };
    
#ifndef NDEBUG
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
#endif
    
    InputChecks.clear();
  }
};

/// Specialization for CIOChecker.
///
template<bool UseCStdLib, int... ArgIs, typename... ArgTs>
struct ArgumentCheckerHandlerImpl
  <false, UseCStdLib, true, seec::ct::sequence_int<ArgIs...>, ArgTs...>
{
  static void impl(seec::trace::TraceProcessListener &Process,
                   seec::trace::TraceThreadListener &Thread,
                   uint32_t const Instruction,
                   seec::runtime_errors::format_selects::CStdFunction const Fn,
                   ArgTs &... Args)
  {
    auto StreamsAccessor = Process.getStreamsAccessor();
    seec::trace::CIOChecker Checker
      {Thread, Instruction, Fn, StreamsAccessor.getObject()};
    
    std::vector<bool> InputChecks
      {ArgumentCheckerDispatch<ArgTs>::impl(Checker, nullptr, Args, ArgIs)...};
    
#ifndef NDEBUG
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
#endif
    
    InputChecks.clear();
  }
};

/// Specialization for CStdLibChecker.
///
template<int... ArgIs, typename... ArgTs>
struct ArgumentCheckerHandlerImpl
  <false, true, false, seec::ct::sequence_int<ArgIs...>, ArgTs...>
{
  static void impl(seec::trace::TraceProcessListener &Process,
                   seec::trace::TraceThreadListener &Thread,
                   uint32_t const Instruction,
                   seec::runtime_errors::format_selects::CStdFunction const Fn,
                   ArgTs &... Args)
  {
    seec::trace::CStdLibChecker Checker{Thread, Instruction, Fn};
    
    std::vector<bool> InputChecks
      {ArgumentCheckerDispatch<ArgTs>::impl(Checker, nullptr, Args, ArgIs)...};
    
#ifndef NDEBUG
    for (auto const InputCheck : InputChecks) {
      assert(InputCheck && "Input check failed.");
    }
#endif
    
    InputChecks.clear();
  }
};

template<typename...>
struct ArgumentCheckerHandler;

template<int... ArgIs, typename... ArgTs>
struct ArgumentCheckerHandler<seec::ct::sequence_int<ArgIs...>, ArgTs...>
: ArgumentCheckerHandlerImpl<
    AnyRequireDIRChecker<ArgTs...>::value,
    AnyRequireChecker<seec::trace::CStdLibChecker, ArgTs...>::value,
    AnyRequireChecker<seec::trace::CIOChecker, ArgTs...>::value,
    seec::ct::sequence_int<ArgIs...>,
    ArgTs...> {};

template<typename RetT,
         typename FnT,
         typename SuccessPredT,
         typename ResultStateRecorderT,
         SimpleWrapperSetting... Settings>
class SimpleWrapperImpl
{
  /// The function that is wrapped.
  seec::runtime_errors::format_selects::CStdFunction FSFunction;

  /// Where did the returned pointer originate from?
  PointerOrigin RetPtrOrigin;

  /// Index of the argument that a returned pointer originated from.
  unsigned RetPtrOriginArg;

  /// \brief Check if the given setting is enabled for this wrapper.
  ///
  template<SimpleWrapperSetting Setting>
  constexpr bool isEnabled() const {
    return isSettingInList<Setting, Settings...>();
  }

  /// \brief Record a new valid pointer.
  ///
  template<typename PtrT>
  typename std::enable_if<std::is_pointer<PtrT>::value, void>::type
  setPointerOriginNewValid(seec::trace::TraceThreadListener &Thread,
                           llvm::Instruction const *Instruction,
                           PtrT const Ptr) const
  {
    auto const PtrInt = reinterpret_cast<uintptr_t>(Ptr);
    auto const MaybeArea = seec::trace::getContainingMemoryArea(Thread, PtrInt);

    // If the referenced area is known then use the start of the area as the
    // pointer's object. Otherwise use the raw pointer value (for opaque
    // pointers, e.g. FILE *).
    auto const Object = MaybeArea.assigned() ? MaybeArea.get<0>().start()
                                             : PtrInt;

    Thread.getActiveFunction()->setPointerObject(
      Instruction,
      Thread.getProcessListener().makePointerObject(Object));
  }

  /// \brief Specialization of above to allow compiling with non-pointer return
  ///        types.
  ///
  void setPointerOriginNewValid(seec::trace::TraceThreadListener &, ...) const
  {}

public:
  /// \brief Construct a new wrapper implementation for the given function.
  ///
  SimpleWrapperImpl(seec::runtime_errors::format_selects::CStdFunction ForFn,
                    PointerOrigin const WithRetPtrOrigin,
                    unsigned const WithRetPtrOriginArg)
  : FSFunction(ForFn),
    RetPtrOrigin(WithRetPtrOrigin),
    RetPtrOriginArg(WithRetPtrOriginArg)
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
    
    Listener.getActiveFunction()->setActiveInstruction(Instruction);

    // Check each of the inputs.
    ArgumentCheckerHandler<seec::ct::sequence_int<ArgIs...>, ArgTs...>
      ::impl(ProcessListener, Listener, InstructionIndex, FSFunction, Args...);
    
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
    ListenerNotifier<RetT> Notifier;
    Notifier(Listener, InstructionIndex, Instruction, Result);

    // Record any changes to errno.
    if (errno != PreCallErrno)
      recordErrno(Listener, errno);
    
    // Record any changes to global variables we are tracking.
    for (auto const &GVTracker : GVTrackers)
      GVTracker.recordChanges(Listener);
    
    // Record state changes revealed by the return value.
    ResultStateRecorder.record(ProcessListener, Listener, Result);
    
    // Record each of the outputs.
    std::vector<bool> OutputRecords {
      (WrappedArgumentRecorder<typename std::remove_reference<ArgTs>::type>
                              (ProcessListener, Listener)
                              .record(Args, Success))...
    };
    
#ifndef NDEBUG
    for (auto const OutputRecord : OutputRecords) {
      assert(OutputRecord && "Output record failed.");
    }
#endif

    // Update the pointer origin (if necessary).
    if (std::is_pointer<RetT>::value) {
      switch (RetPtrOrigin) {
        case PointerOrigin::None:
          // TODO: This is bad.
          break;
        case PointerOrigin::FromArgument:
          Listener.getActiveFunction()
                    ->transferArgPointerObjectToCall(RetPtrOriginArg);
          break;
        case PointerOrigin::NewValid:
          setPointerOriginNewValid(Listener, Instruction, Result);
          break;
      }
    }
    
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
  constexpr bool isEnabled() const {
    return isSettingInList<Setting, Settings...>();
  }
  
public:
  /// \brief Construct a new wrapper implementation for the given function.
  ///
  SimpleWrapperImpl(seec::runtime_errors::format_selects::CStdFunction ForFn,
                    PointerOrigin const WithRetPtrOrigin,
                    unsigned const WithRetPtrOriginArg)
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
    auto &ProcessEnv = seec::trace::getProcessEnvironment();
    auto &ProcessListener = ProcessEnv.getProcessListener();
    
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
    
    auto const CallInstruction = ThreadEnv.getInstruction();
    Listener.getActiveFunction()->setActiveInstruction(CallInstruction);

    // Check each of the inputs.
    ArgumentCheckerHandler<seec::ct::sequence_int<ArgIs...>, ArgTs...>
      ::impl(ProcessListener, Listener, InstructionIndex, FSFunction, Args...);
    
    // Get the pre-call value of errno.
    auto const PreCallErrno = errno;
    
    // Get the pre-call values of all global variables we are tracking.
    for (auto &GVTracker : GVTrackers)
      GVTracker.savePreCallState();
    
    // Call the original function.
    Function(std::forward<ArgTs>(Args)...);
    bool const Success = SuccessPred();
        
    // Record any changes to errno.
    if (errno != PreCallErrno)
      recordErrno(Listener, errno);
    
    // Record any changes to global variables we are tracking.
    for (auto const &GVTracker : GVTrackers)
      GVTracker.recordChanges(Listener);
    
    // Record state changes revealed by the return value.
    ResultStateRecorder.record(ProcessListener, Listener);
    
    // Record each of the outputs.
    std::vector<bool> OutputRecords {
      (WrappedArgumentRecorder<typename std::remove_reference<ArgTs>::type>
                              (ProcessListener, Listener)
                              .record(Args, Success))...
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

  /// Where did the returned pointer originate from?
  PointerOrigin RetPtrOrigin;

  /// Index of the argument that a returned pointer originated from.
  unsigned RetPtrOriginArg;

  /// @}
  
public:
  /// \brief Construct a new wrapper.
  ///
  SimpleWrapper(seec::runtime_errors::format_selects::CStdFunction ForFunction)
  : FSFunction(ForFunction),
    GVTrackers(),
    RetPtrOrigin(PointerOrigin::None),
    RetPtrOriginArg(0)
  {}
  
  /// \brief Add a global variable tracker.
  ///
  template<typename T>
  SimpleWrapper &trackGlobal(T const &Global)
  {
    GVTrackers.push_back(GlobalVariableTracker{Global});
    return *this;
  }

  /// \brief Set the pointer to originate from an argument.
  ///
  SimpleWrapper &returnPointerFromArg(unsigned const ArgNo)
  {
    RetPtrOrigin = PointerOrigin::FromArgument;
    RetPtrOriginArg = ArgNo;
    return *this;
  }

  /// \brief Set the returned pointer to be newly created and valid.
  ///
  SimpleWrapper &returnPointerIsNewAndValid()
  {
    RetPtrOrigin = PointerOrigin::NewValid;
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
                             { FSFunction,
                               RetPtrOrigin,
                               RetPtrOriginArg }
            .impl(std::forward<FnT>(Function),
                  std::forward<SuccessPredT>(SuccessPred),
                  std::forward<ResultStateRecorderT>(ResultStateRecorder),
                  GVTrackers,
                  Indices,
                  std::forward<ArgT>(Args)...);
  }
};


} // namespace seec
