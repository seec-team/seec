#ifndef SEEC_TRACE_TRACETHREADMEMCHECK_HPP
#define SEEC_TRACE_TRACETHREADMEMCHECK_HPP

#include "seec/DSA/MemoryArea.hpp"
#include "seec/RuntimeErrors/FormatSelects.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdint>

namespace seec {

namespace trace {

/// \brief Get the allocated memory area that contains Address, if any.
/// \param Listener the listener for the thread requesting this information.
/// \param Address the address of memory to find the owning allocation for.
seec::util::Maybe<MemoryArea>
getContainingMemoryArea(TraceThreadListener &Listener, uintptr_t Address);

/// \brief If Str points to a C string that fits within Area, then find the
///        area of that C string, including the terminating nul character.
seec::util::Maybe<MemoryArea>
getCStringInArea(char const *Str, MemoryArea Area);

/// \brief Raise an InvalidCString error if the CStringArea is unassigned.
bool checkCStringIsValid(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        uintptr_t Address,
        uint64_t ParameterIndex,
        seec::runtime_errors::format_selects::CStdFunction Function,
        seec::util::Maybe<MemoryArea> CStringArea
        );

/// \brief Raise a MemoryUnowned error if the ContainingArea is unassigned.
///
/// \param Listener the listener for the thread that this check is occuring in.
/// \param InstructionIndex the index of the current llvm::Instruction.
/// \param Address the start address of the memory access that we're checking.
/// \param Size the size of the memory access that we're checking.
/// \param Access the type of memory access that we're checking.
/// \param ContainingArea the memory area containing Address, if any.
/// \return true iff the memory was unowned (ContainingArea was not assigned).
bool checkMemoryOwnership(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        uintptr_t Address,
        std::size_t Size,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        seec::util::Maybe<MemoryArea> ContainingArea);

/// \brief Raise a PassPointerToUnowned error if the ContainingArea is
///        unassigned.
///
/// \param Listener the listener for the thread that this check is occuring in.
/// \param InstructionIndex the index of the current llvm::Instruction.
/// \param Function the function that is being passed a pointer to Address.
/// \param ParameterIndex the parameter that the pointer was passed in.
/// \param Access the type of memory access that we're checking.
/// \param Address the start address of the memory access that we're checking.
/// \param Size the size of the memory access that we're checking.
/// \param ContainingArea the memory area containing Address, if any.
/// \return true iff the memory was unowned (ContainingArea was not assigned).
bool checkMemoryOwnershipOfParameter(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        seec::runtime_errors::format_selects::CStdFunction Function,
        std::size_t ParameterIndex,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        uintptr_t Address,
        std::size_t Size,
        seec::util::Maybe<MemoryArea> ContainingArea);

/// \brief Check whether or not a memory access is valid.
///
/// Checks whether the size of the ContainingArea is sufficient for the memory
/// access. If the Access is a read, checks whether the memory is initialized.
///
/// \param Listener the listener for the thread that this check is occuring in.
/// \param InstructionIndex the index of the current llvm::Instruction.
/// \param Address the start address of the memory access that we're checking.
/// \param Size the size of the memory access that we're checking.
/// \param Access the type of memory access that we're checking.
/// \param ContainingArea the memory area containing Address.
/// \return true iff an error was detected.
bool checkMemoryAccess(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        uintptr_t Address,
        std::size_t Size,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        MemoryArea ContainingArea);

/// \brief Check whether or not dereferencing a parameter to a standard
///        function would be valid.
///
/// Checks whether the size of the ContainingArea is sufficient for the memory
/// access. If the Access is a read, checks whether the memory is initialized.
///
/// \param Listener the listener for the thread that this check is occuring in.
/// \param InstructionIndex the index of the current llvm::Instruction.
/// \param Function the function that is being passed a pointer to Address.
/// \param ParameterIndex the parameter that the pointer was passed in.
/// \param Access the type of memory access that we're checking.
/// \param Address the start address of the memory access that we're checking.
/// \param Size the size of the memory access that we're checking.
/// \param ContainingArea the memory area containing Address.
/// \return true iff an error was detected.
bool checkMemoryAccessOfParameter(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        seec::runtime_errors::format_selects::CStdFunction Function,
        std::size_t ParameterIndex,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        uintptr_t Address,
        std::size_t Size,
        MemoryArea ContainingArea);

/// \brief Check whether or not a memory access is valid.
/// 
/// This function first checks to see whether or not the memory at the given
/// address is owned, using checkMemoryOwnership(). If the memory is unowned,
/// an error will be raised and this function will return. If the memory is
/// owned, then the access will be checked using checkMemoryAccess().
///
/// \tparam Access the type of memory access.
/// 
/// \param Listener the listener for the thread that this check is occuring in.
/// \param Address the start address of the memory access that we're checking.
/// \param Size the size of the memory access that we're checking.
/// \param InstructionIndex the index of the current llvm::Instruction.
template<seec::runtime_errors::format_selects::MemoryAccess Access>
void checkMemoryAccess(TraceThreadListener &Listener,
                       uintptr_t Address,
                       std::size_t Size,
                       uint32_t InstrIndex) {
  auto MaybeArea = getContainingMemoryArea(Listener, Address);

  if (checkMemoryOwnership(Listener,
                           InstrIndex,
                           Address,
                           Size,
                           Access,
                           MaybeArea)) {
    return;
  }

  checkMemoryAccess(Listener,
                    InstrIndex,
                    Address,
                    Size,
                    Access,
                    MaybeArea.get<0>());
}

/// \brief Check if the pointer passed to a standard function is valid.
///
/// \param Listener the listener for the thread that this check is occuring in.
/// \param InstructionIndex the index of the current llvm::Instruction.
/// \param Function the function that is being passed a pointer to Address.
/// \param ParameterIndex the parameter that the pointer was passed in.
/// \param Access the type of memory access that we're checking.
/// \param Address the start address of the memory access that we're checking.
/// \param Size the size of the memory access that we're checking.
inline void checkMemoryAccessOfParameter(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        seec::runtime_errors::format_selects::CStdFunction Function,
        std::size_t ParameterIndex,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        uintptr_t Address,
        std::size_t Size) {
  auto MaybeArea = getContainingMemoryArea(Listener, Address);
  
  if (checkMemoryOwnershipOfParameter(Listener,
                                      InstructionIndex,
                                      Function,
                                      ParameterIndex,
                                      Access,
                                      Address,
                                      Size,
                                      MaybeArea)) {
    return;
  }

  checkMemoryAccessOfParameter(Listener,
                               InstructionIndex,
                               Function,
                               ParameterIndex,
                               Access,
                               Address,
                               Size,
                               MaybeArea.get<0>());
}

///
template<seec::runtime_errors::format_selects::CStdFunction FuncT>
void checkMemoryOverlap(TraceThreadListener &Listener,
                        uint32_t InstructionIndex,
                        MemoryArea Area1,
                        MemoryArea Area2) {
  using namespace seec::runtime_errors;

  auto OverlapArea = Area1.intersection(Area2);
  if (!OverlapArea.length())
    return;

  Listener.handleRunError(
    createRunError<RunErrorType::OverlappingSourceDest>(FuncT,
                                                        OverlapArea.start(),
                                                        OverlapArea.length()),
    RunErrorSeverity::Warning,
    InstructionIndex);
}

/// \brief Check if a character pointer points to an owned area of memory,
/// which can be read from and contains a nul-terminated C string.
///
/// If an error is detected, then a runtime error will be raised in Listener,
/// and this function will return true.
///
/// \param Listener The thread that the check will occur in (any runtime errors
///                 will be stored in this thread.
/// \param InstructionIndex The index of the llvm::Instruction that is reading
///                         from this pointer.
/// \param Func The string function that is reading from this pointer.
/// \param ParameterIndex The index of Str in Func's argument list.
/// \param Str The pointer that will be read as a string.
/// \return true if an error was detected.
///
bool checkCStringRead(TraceThreadListener &Listener,
                      uint32_t InstructionIndex,
                      seec::runtime_errors::format_selects::CStdFunction Func,
                      uint64_t ParameterIndex,
                      char const *Str);

///
bool checkLimitedCStringRead(
                      TraceThreadListener &Listener,
                      uint32_t InstructionIndex,
                      seec::runtime_errors::format_selects::CStdFunction Func,
                      uint64_t ParameterIndex,
                      char const *Str,
                      std::size_t Limit);

/// \brief Helps detect and report run-time errors with C stdlib usage.
class CStdLibChecker {
  /// The listener for the thread we are checking.
  TraceThreadListener &Thread;
  
  /// The index of the llvm::Instruction we are checking.
  uint32_t Instruction;
  
public:
  /// \brief Constructor.
  /// \param InThread The listener for the thread we are checking.
  /// \param InstructionIndex Index of the llvm::Instruction we are checking.
  CStdLibChecker(TraceThreadListener &InThread,
                 uint32_t InstructionIndex)
  : Thread(InThread),
    Instruction(InstructionIndex)
  {}
  
  /// \brief Find the limited C string referenced by String.
  /// If String points to a C string that fits within Area, then get the
  /// area of that C string, including the terminating nul character. If
  /// there is no C string in the Area, or the string is longer than
  /// Limit, then return the area [String, String + Limit).
  MemoryArea getLimitedCStringInArea(char const *String,
                                     MemoryArea Area,
                                     std::size_t Limit);
  
  /// \brief Check a size-limited read from a C String.
  ///
  /// \return The number of characters in the string that can be read.
  std::size_t checkLimitedCStringRead(
                seec::runtime_errors::format_selects::CStdFunction Function,
                unsigned Parameter,
                char const *String,
                std::size_t Limit);
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACETHREADMEMCHECK_HPP
