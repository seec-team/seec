#ifndef SEEC_TRACE_TRACETHREADMEMCHECK_HPP
#define SEEC_TRACE_TRACETHREADMEMCHECK_HPP

#include "seec/DSA/MemoryArea.hpp"
#include "seec/RuntimeErrors/FormatSelects.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Util/Maybe.hpp"

#include <cstdint>

namespace seec {

namespace trace {

/// \brief Get the allocated memory area that contains Address, if any.
seec::util::Maybe<MemoryArea>
getContainingMemoryArea(TraceThreadListener &Listener, uint64_t Address);

/// \brief If Str points to a C string that fits within Area, then find the
///        area of that C string, including the terminating nul character.
seec::util::Maybe<MemoryArea>
getCStringInArea(char const *Str, MemoryArea Area);

/// \brief Raise an InvalidCString error if the CStringArea is unassigned.
bool checkCStringIsValid(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        uint64_t Address,
        uint64_t ParameterIndex,
        seec::runtime_errors::format_selects::StringFunction Function,
        seec::util::Maybe<MemoryArea> CStringArea
        );

///
/// \return true iff the memory was unowned (ContainingArea was not assigned).
bool checkMemoryOwnership(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        uint64_t Address,
        uint64_t Size,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        seec::util::Maybe<MemoryArea> ContainingArea);

/// \brief Check whether or not a memory access is valid.
/// Checks whether the size of the ContainingArea is sufficient for the memory
/// access. If the Access is a read, checks whether the memory is initialized.
/// \return true iff an error was detected.
bool checkMemoryAccess(
        TraceThreadListener &Listener,
        uint32_t InstructionIndex,
        uint64_t Address,
        uint64_t Size,
        seec::runtime_errors::format_selects::MemoryAccess Access,
        MemoryArea ContainingArea);

///
template<seec::runtime_errors::format_selects::MemoryAccess Access>
void checkMemoryAccess(TraceThreadListener &Listener,
                       uint64_t Address,
                       uint64_t Size,
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

///
template<seec::runtime_errors::format_selects::MemCopyFunction FuncT>
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
                      seec::runtime_errors::format_selects::StringFunction Func,
                      uint64_t ParameterIndex,
                      char const *Str);

///
bool checkLimitedCStringRead(
                      TraceThreadListener &Listener,
                      uint32_t InstructionIndex,
                      seec::runtime_errors::format_selects::StringFunction Func,
                      uint64_t ParameterIndex,
                      char const *Str,
                      std::size_t Limit);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACETHREADMEMCHECK_HPP
