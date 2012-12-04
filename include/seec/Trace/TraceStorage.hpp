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

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"

#include <memory>

namespace seec {

namespace trace {

enum class ProcessSegment {
  Trace = 1,
  Data
};

enum class ThreadSegment {
  Trace = 1,
  Events
};

/// \brief Allocates raw_ostreams for the various outputs required by tracing.
///
/// This gives us a central area to control the output locations and filenames.
class OutputStreamAllocator {
public:
  OutputStreamAllocator() {}

  /// Get an output for a process-level data segment.
  std::unique_ptr<llvm::raw_ostream> getProcessStream(ProcessSegment Segment);

  /// Get an output for a thread-specific data segment.
  std::unique_ptr<llvm::raw_ostream> getThreadStream(uint32_t ThreadID,
                                                     ThreadSegment Segment);
};

/// Gets MemoryBuffers for the various sections of a trace.
class InputBufferAllocator {
  llvm::sys::Path TraceDirectory;

public:
  /// Default constructor.
  InputBufferAllocator()
  : TraceDirectory(llvm::sys::Path::GetCurrentDirectory())
  {}

  /// Construct an allocator that loads from the specified directory.
  InputBufferAllocator(llvm::sys::Path FromDirectory)
  : TraceDirectory(FromDirectory)
  {
    assert(TraceDirectory.canRead());
  }

  /// Copy constructor.
  InputBufferAllocator(InputBufferAllocator const &) = default;

  /// Copy assignment.
  InputBufferAllocator &operator=(InputBufferAllocator const &) = default;

  /// Move constructor.
  InputBufferAllocator(InputBufferAllocator &&Other)
  : TraceDirectory(std::move(Other.TraceDirectory))
  {}

  /// Move assignment.
  InputBufferAllocator &operator=(InputBufferAllocator &&RHS) {
    TraceDirectory = std::move(RHS.TraceDirectory);
    return *this;
  }
  
  /// \brief Get the path of the directory containing the trace files.
  ///
  llvm::sys::Path const &getTraceDirectory() const { return TraceDirectory; }

  /// Create a MemoryBuffer containing the process data for the given Segment.
  seec::util::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getProcessData(ProcessSegment Segment);

  /// Create a MemoryBuffer containing the data for the given thread's given
  /// Segment.
  seec::util::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getThreadData(uint32_t ThreadID, ThreadSegment Segment);
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTORAGE_HPP
