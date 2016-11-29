//===- lib/Trace/TraceThreadMemCheck.hpp ----------------------------------===//
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

#ifndef SEEC_TRACE_TRACETHREADMEMCHECK_HPP
#define SEEC_TRACE_TRACETHREADMEMCHECK_HPP

#include "seec/DSA/MemoryArea.hpp"
#include "seec/RuntimeErrors/FormatSelects.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TracePointer.hpp"
#include "seec/Trace/TraceProcessListener.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <memory>

namespace seec {

namespace trace {

/// \brief Get the allocated memory area that contains Address, if any.
/// \param Listener the listener for the thread requesting this information.
/// \param Address the address of memory to find the owning allocation for.
seec::Maybe<MemoryArea>
getContainingMemoryArea(TraceThreadListener &Listener, uintptr_t Address);

/// \brief Helps detect and report run-time errors with memory usage.
class RuntimeErrorChecker {
protected:
  /// The listener for the thread we are checking.
  TraceThreadListener &Thread;
  
  /// The index of the llvm::Instruction we are checking.
  InstrIndexInFn const Instruction;

  /// These will be attached to any produced RunError.
  std::vector<std::unique_ptr<runtime_errors::RunError>> PermanentNotes;

  /// These will be attached to any produced RunError.
  std::vector<std::unique_ptr<runtime_errors::RunError>> TemporaryNotes;

  /// \brief Raises the given \c RunError in our thread.
  /// All \c PermanentNotes and \c TemporaryNotes will be cloned and attached
  /// to \c Err as additional errors.
  ///
  void raiseError(runtime_errors::RunError &Err,
                  RunErrorSeverity const Severity);

  /// \brief Add a permanent note.
  /// This will be attached as an additional error to all future \c RunError
  /// errors raised by this checker.
  ///
  void addPermanentNote(std::unique_ptr<runtime_errors::RunError> Note);

  /// \brief Add a temporary note.
  /// This will be attached as an additional error to future \c RunError errors
  /// raised by this checker, until the temporary notes are cleared.
  ///
  void addTemporaryNote(std::unique_ptr<runtime_errors::RunError> Note);

  /// \brief Clear all temporary notes.
  ///
  void clearTemporaryNotes();

public:
  /// \brief Constructor.
  /// \param ForThread The thread we are checking.
  /// \param ForInstruction Index of the llvm::Instruction we are checking.
  ///
  RuntimeErrorChecker(TraceThreadListener &ForThread,
                      InstrIndexInFn ForInstruction)
  : Thread(ForThread),
    Instruction(ForInstruction),
    PermanentNotes(),
    TemporaryNotes()
  {}

  /// \brief Find the number of owned/known bytes starting at Address.
  ///
  std::ptrdiff_t getSizeOfAreaStartingAt(uintptr_t Address);
  
  /// \brief Find the number of writable owned/known bytes starting at Address.
  ///
  std::ptrdiff_t getSizeOfWritableAreaStartingAt(uintptr_t Address);

  /// \brief Check that a pointer is valid to dereference.
  ///
  bool checkPointer(PointerTarget const &PtrObj, uintptr_t const Address);

  /// \brief Create a MemoryUnowned runtime error if Area is unassigned.
  ///
  /// \return true if Area is assigned (no runtime error was created).
  bool memoryExists(uintptr_t Address,
                    std::size_t Size,
                    seec::runtime_errors::format_selects::MemoryAccess Access,
                    seec::Maybe<MemoryArea> const &Area);
  
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
  seec::Maybe<MemoryArea> getCStringInArea(char const *String,
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

  /// Index of the calling function's \c TracedFunction in the shadow stack.
  unsigned const CallerIdx;

  /// The call to this Function.
  llvm::CallInst const *Call;

  /// \brief Create a PassPointerToUnowned runtime error if Area is unassigned.
  ///
  /// \return true if Area is assigned (no runtime error was created).
  ///
  bool memoryExistsForParameter(
          unsigned Parameter,
          uintptr_t Address,
          std::size_t Size,
          seec::runtime_errors::format_selects::MemoryAccess Access,
          seec::Maybe<MemoryArea> const &Area,
          PointerTarget const &PtrObj);

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

  /// \brief Create an InvalidCString error if Area is unassigned.
  ///
  /// \return true iff there were no errors.
  bool checkCStringIsValid(char const *String,
                           unsigned Parameter,
                           seec::Maybe<MemoryArea> Area);

  /// \brief Check a read from a C String.
  ///
  /// \return The number of characters in the string that can be read,
  ///         including the terminating nul byte. Zero indicates that nothing
  ///         can be read (in which case a runtime error has been raised).
  ///
  std::size_t checkCStringRead(unsigned Parameter,
                               char const *String,
                               PointerTarget const &PtrObj);

public:
  /// \brief Constructor.
  /// \param InThread The listener for the thread we are checking.
  /// \param InstructionIndex Index of the llvm::Instruction we are checking.
  /// \param Function the function we are checking.
  CStdLibChecker(TraceThreadListener &InThread,
                 InstrIndexInFn InstructionIndex,
                 seec::runtime_errors::format_selects::CStdFunction Function);

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

  /// \brief Check a read from a C String.
  ///
  /// \return The number of characters in the string that can be read,
  ///         including the terminating nul byte. Zero indicates that nothing
  ///         can be read (in which case a runtime error has been raised).
  ///
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

  /// \brief Check that an array of C Strings is valid and NULL-terminated.
  ///
  /// \return The number of elements in the array, including the terminating
  ///         NULL pointer. Zero indicates that no elements are accessible (in
  ///         which case a runtime error has been raised).
  std::size_t checkCStringArray(unsigned Parameter, char const * const *Array);

  /// \brief Check the validity of a print format string.
  ///
  /// \return true iff there were no errors.
  ///
  bool
  checkPrintFormat(unsigned Parameter,
                   char const *String,
                   detect_calls::VarArgList<TraceThreadListener> const &Args);
};


/// \brief Helps detect and report run-time errors with I/O stream usage.
class CIOChecker : public CStdLibChecker {
  TraceStreams const &Streams;

public:
  CIOChecker(TraceThreadListener &InThread,
             InstrIndexInFn InstructionIndex,
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


/// \brief Helps detect and report errors with DIR usage.
///
class DIRChecker {
  TraceThreadListener &Thread;
  
  InstrIndexInFn const InstructionIndex;
  
  seec::runtime_errors::format_selects::CStdFunction const Function;
  
  TraceDirs const &Dirs;
  
public:
  DIRChecker(TraceThreadListener &InThread,
             InstrIndexInFn WithInstructionIndex,
             seec::runtime_errors::format_selects::CStdFunction ForFunction,
             TraceDirs const &WithDirs)
  : Thread(InThread),
    InstructionIndex(WithInstructionIndex),
    Function(ForFunction),
    Dirs(WithDirs)
  {}
  
  /// \brief Check if a DIR * is valid.
  ///
  /// \return true iff there were no errors.
  bool checkDIRIsValid(unsigned const Parameter,
                       void const * const TheDIR);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACETHREADMEMCHECK_HPP
