//===- include/seec/Trace/TraceStorage.hpp -------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACESTORAGE_HPP
#define SEEC_TRACE_TRACESTORAGE_HPP

#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>


namespace llvm {
  class LLVMContext;
  class Module;
}


namespace seec {

namespace trace {


/// Enumerates process-level data segment types.
///
enum class ProcessSegment {
  Trace = 1,
  Data
};


/// Enumerates thread-level data segment types.
///
enum class ThreadSegment {
  Trace = 1,
  Events
};


/// \brief Allocates raw_ostreams for the various outputs required by tracing.
///
/// This gives us a central area to control the output locations and filenames.
class OutputStreamAllocator {
  /// Path to the trace directory.
  std::string TraceDirectory;
  
  /// \brief Create a new OutputStreamAllocator.
  ///
  OutputStreamAllocator(llvm::StringRef Directory);
  
  // Don't allow copying.
  OutputStreamAllocator(OutputStreamAllocator const &) = delete;
  OutputStreamAllocator(OutputStreamAllocator &&) = delete;
  OutputStreamAllocator &operator=(OutputStreamAllocator const &) = delete;
  OutputStreamAllocator &operator=(OutputStreamAllocator &&) = delete;
  
public:
  /// \brief Attempt to create OutputStreamAllocator.
  ///
  static 
  seec::Maybe<std::unique_ptr<OutputStreamAllocator>, seec::Error>
  createOutputStreamAllocator();
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Write the Module's bitcode to the trace directory.
  ///
  seec::Maybe<seec::Error> writeModule(llvm::StringRef Bitcode);
  
  /// \brief Get an output for a process-level data segment.
  ///
  std::unique_ptr<llvm::raw_ostream>
  getProcessStream(ProcessSegment Segment,
                   unsigned Flags = 0);

  /// \brief Get an output for a thread-specific data segment.
  ///
  std::unique_ptr<llvm::raw_ostream>
  getThreadStream(uint32_t ThreadID,
                  ThreadSegment Segment,
                  unsigned Flags = 0);
  
  /// @}
};


/// \brief Gets MemoryBuffers for the various sections of a trace.
///
class InputBufferAllocator {
  std::string TraceDirectory;

  /// Default constructor.
  InputBufferAllocator(llvm::StringRef Directory)
  : TraceDirectory(Directory)
  {}
  
public:
  /// \name Constructors.
  /// @{
  
  /// \brief Attempt to create an InputBufferAllocator.
  ///
  /// \param Directory the path to the trace directory.
  /// \return The InputBufferAllocator or an Error describing the reason why it
  ///         could not be created.
  ///
  static
  seec::Maybe<InputBufferAllocator, seec::Error>
  createFor(llvm::StringRef Directory);

  /// Copy constructor.
  InputBufferAllocator(InputBufferAllocator const &) = default;

  /// Move constructor.
  InputBufferAllocator(InputBufferAllocator &&Other) = default;

  /// @} (Constructors.)
  

  /// \name Operators.
  /// @{
  
  /// Copy assignment.
  InputBufferAllocator &operator=(InputBufferAllocator const &) = default;
  
  /// Move assignment.
  InputBufferAllocator &operator=(InputBufferAllocator &&RHS) = default;
  
  /// @} (Operators.)
  
  
  /// \brief Get the path of the directory containing the trace files.
  ///
  std::string const &getTraceDirectory() const {
    return TraceDirectory;
  }
  
  /// \brief Get the original, uninstrumented Module.
  ///
  seec::Maybe<llvm::Module *, seec::Error>
  getModule(llvm::LLVMContext &Context);

  /// Create a MemoryBuffer containing the process data for the given Segment.
  ///
  seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getProcessData(ProcessSegment Segment);

  /// Create a MemoryBuffer containing the data for the given thread's given
  /// Segment.
  ///
  seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getThreadData(uint32_t ThreadID, ThreadSegment Segment);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTORAGE_HPP
