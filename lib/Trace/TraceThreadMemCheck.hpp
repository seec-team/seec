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

/// \brief Helps detect and report run-time errors with memory usage.
class RuntimeErrorChecker {
protected:
  /// The listener for the thread we are checking.
  TraceThreadListener &Thread;
  
  /// The index of the llvm::Instruction we are checking.
  uint32_t const Instruction;
  
public:
  /// \brief Constructor.
  /// \param ForThread The thread we are checking.
  /// \param ForInstruction Index of the llvm::Instruction we are checking.
  RuntimeErrorChecker(TraceThreadListener &ForThread,
                      uint32_t ForInstruction)
  : Thread(ForThread),
    Instruction(ForInstruction)
  {}
  
  /// \brief Create a MemoryUnowned runtime error if Area is unassigned.
  ///
  /// \return true if Area is assigned (no runtime error was created).
  bool memoryExists(uintptr_t Address,
                    std::size_t Size,
                    seec::runtime_errors::format_selects::MemoryAccess Access,
                    seec::util::Maybe<MemoryArea> const &Area);
  
  /// \brief Check whether or not a memory access is valid.
  ///
  /// Checks whether the size of the ContainingArea is sufficient for the
  /// memory access. If the Access is a read, checks whether the memory is
  /// initialized.
  ///
  /// \param Address the start address of the memory access that we're checking.
  /// \param Size the size of the memory access that we're checking.
  /// \param Access the type of memory access that we're checking.
  /// \param ContainingArea the memory area containing Address.
  ///
  /// \return true iff there were no errors.
  bool
  checkMemoryAccess(uintptr_t Address,
                    std::size_t Size,
                    seec::runtime_errors::format_selects::MemoryAccess Access,
                    MemoryArea ContainingArea);
  
  /// \brief Check if memory is known and accessible.
  ///
  /// \return true iff there were no errors.
  bool
  checkMemoryExistsAndAccessible(
                    uintptr_t Address,
                    std::size_t Size,
                    seec::runtime_errors::format_selects::MemoryAccess Access);
  
  /// \brief Find the area of the C string referenced by String.
  seec::util::Maybe<MemoryArea> getCStringInArea(char const *String,
                                                 MemoryArea Area);

  /// \brief Find the limited C string referenced by String.
  /// If String points to a C string that fits within Area, then get the
  /// area of that C string, including the terminating nul character. If
  /// there is no C string in the Area, or the string is longer than
  /// Limit, then return the area [String, String + Limit).
  MemoryArea getLimitedCStringInArea(char const *String,
                                     MemoryArea Area,
                                     std::size_t Limit);
};

/// \brief Helps detect and report run-time errors with C stdlib usage.
class CStdLibChecker : public RuntimeErrorChecker {
protected:
  /// The function that we are checking.
  seec::runtime_errors::format_selects::CStdFunction const Function;
  
public:
  /// \brief Constructor.
  /// \param InThread The listener for the thread we are checking.
  /// \param InstructionIndex Index of the llvm::Instruction we are checking.
  /// \param Function the function we are checking.
  CStdLibChecker(TraceThreadListener &InThread,
                 uint32_t InstructionIndex,
                 seec::runtime_errors::format_selects::CStdFunction Function)
  : RuntimeErrorChecker(InThread, InstructionIndex),
    Function(Function)
  {}
  
  /// \brief Create a PassPointerToUnowned runtime error if Area is unassigned.
  ///
  /// \return true if Area is assigned (no runtime error was created).
  bool memoryExistsForParameter(
          unsigned Parameter,
          uintptr_t Address,
          std::size_t Size,
          seec::runtime_errors::format_selects::MemoryAccess Access,
          seec::util::Maybe<MemoryArea> const &Area);
  
  /// \brief Check whether or not a memory access is valid.
  ///
  /// Checks whether the size of the ContainingArea is sufficient for the
  /// memory access. If the Access is a read, checks whether the memory is
  /// initialized.
  ///
  /// \param Address the start address of the memory access that we're checking.
  /// \param Size the size of the memory access that we're checking.
  /// \param Access the type of memory access that we're checking.
  /// \param ContainingArea the memory area containing Address.
  ///
  /// \return true iff there were no errors.
  bool checkMemoryAccessForParameter(
          unsigned Parameter,
          uintptr_t Address,
          std::size_t Size,
          seec::runtime_errors::format_selects::MemoryAccess Access,
          MemoryArea ContainingArea);
  
  /// \brief Check if memory is known and accessible.
  ///
  /// \return true iff there were no errors.
  bool checkMemoryExistsAndAccessibleForParameter(
          unsigned Parameter,
          uintptr_t Address,
          std::size_t Size,
          seec::runtime_errors::format_selects::MemoryAccess Access);
  
  /// \brief Create a runtime error if two memory areas overlap.
  ///
  /// \return true iff the memory areas do not overlap.
  bool checkMemoryDoesNotOverlap(MemoryArea Area1, MemoryArea Area2);
  
  /// \brief Create an InvalidCString error if Area is unassigned.
  ///
  /// \return true iff there were no errors.
  bool checkCStringIsValid(uintptr_t Address,
                           unsigned Parameter,
                           seec::util::Maybe<MemoryArea> Area);
  
  /// \brief Check a read from a C String.
  ///
  /// \return The number of characters in the string that can be read,
  ///         including the terminating nul byte. Zero indicates that nothing
  ///         can be read (in which case a runtime error has been raised).
  std::size_t checkCStringRead(unsigned Parameter,
                               char const *String);
  
  /// \brief Check a size-limited read from a C String.
  ///
  /// \return The number of characters in the string that can be read,
  ///         including the terminating nul byte. Zero indicates that nothing
  ///         can be read (in which case a runtime error has been raised).
  std::size_t checkLimitedCStringRead(unsigned Parameter,
                                      char const *String,
                                      std::size_t Limit);
  
  /// \brief Check the validity of a print format string.
  ///
  /// \return true iff there were no errors.
  ///
  bool
  checkPrintFormat(unsigned Parameter,
                   char const *String,
                   detect_calls::VarArgList<TraceThreadListener> const &Args);
  
  /// \brief Check the validity of a scan format string.
  ///
  /// \return true iff there were no errors.
  ///
  bool
  checkScanFormat(unsigned Parameter,
                  char const *String,
                  detect_calls::VarArgList<TraceThreadListener> const &Args);
};


/// \brief Helps detect and report run-time errors with I/O stream usage.
class CIOChecker : public CStdLibChecker {
  TraceStreams const &Streams;

public:
  CIOChecker(TraceThreadListener &InThread,
             uint32_t InstructionIndex,
             seec::runtime_errors::format_selects::CStdFunction Function,
             TraceStreams const &StreamsInfo)
  : CStdLibChecker(InThread, InstructionIndex, Function),
    Streams(StreamsInfo)
  {}
  
  /// \brief Check if a FILE * is valid.
  ///
  /// \return true iff there were no errors.
  bool checkStreamIsValid(unsigned Parameter,
                          FILE *Stream);
  
  /// \brief Check if a standard stream is valid.
  ///
  /// \return true iff there were no errors.
  bool checkStandardStreamIsValid(FILE *Stream);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACETHREADMEMCHECK_HPP
