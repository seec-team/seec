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
#include "llvm/Support/system_error.h"

#include <memory>
#include <string>
#include <vector>


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
///
class OutputStreamAllocator {
  /// Path to the trace location.
  std::string TraceLocation;
  
  /// Name of the directory containing the trace files.
  std::string TraceDirectoryName;
  
  /// Path to the directory containing the trace files.
  std::string TraceDirectoryPath;
  
  /// Name to use when creating an archive of the trace.
  std::string TraceArchiveName;
  
  /// Paths for all created files.
  std::vector<std::string> TraceFiles;
  
  /// \brief Create a new OutputStreamAllocator.
  ///
  OutputStreamAllocator(llvm::StringRef WithTraceLocation,
                        llvm::StringRef WithTraceDirectoryName,
                        llvm::StringRef WithTraceDirectoryPath,
                        llvm::StringRef WithTraceArchiveName);
  
  // Don't allow copying.
  OutputStreamAllocator(OutputStreamAllocator const &) = delete;
  OutputStreamAllocator(OutputStreamAllocator &&) = delete;
  OutputStreamAllocator &operator=(OutputStreamAllocator const &) = delete;
  OutputStreamAllocator &operator=(OutputStreamAllocator &&) = delete;
  
  /// \brief Attempt to delete all existing trace files, and the directory.
  ///
  bool deleteAll();
  
public:
  /// \brief Construction.
  /// @{
  
  /// \brief Attempt to create OutputStreamAllocator.
  ///
  static 
  seec::Maybe<std::unique_ptr<OutputStreamAllocator>, seec::Error>
  createOutputStreamAllocator();
  
  /// @} (Construction.)
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the combined size of all trace files (in bytes).
  ///
  seec::Maybe<uint64_t, seec::Error> getTotalSize() const;
  
  /// @} (Accessors.)
  
  
  /// \name Mutators.
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
  
  /// \brief Archive all existing trace files.
  ///
  /// If the files are successfully archived, the originals will be deleted.
  ///
  seec::Maybe<seec::Error> archiveTo(llvm::StringRef Path);
  
  /// @} (Mutators.)
};


/// \brief Holds the name and contents of a single file in a trace.
///
class TraceFile {
  /// The name of the file.
  std::string Name;
  
  /// The contents of the file.
  std::unique_ptr<llvm::MemoryBuffer> Contents;
  
public:
  /// \brief Constructor.
  ///
  TraceFile(std::string WithName,
            std::unique_ptr<llvm::MemoryBuffer> WithContents)
  : Name(std::move(WithName)),
    Contents(std::move(WithContents))
  {}
  
  /// \brief Get the name of the file.
  ///
  decltype(Name) const &getName() const { return Name; }
  
  /// \brief Get the contents of the file.
  ///
  decltype(Contents) &getContents() { return Contents; }
  
  /// \brief Get the contents of the file.
  ///
  decltype(Contents) const &getContents() const { return Contents; }
};


/// \brief Gets MemoryBuffers for the various sections of a trace.
///
class InputBufferAllocator {
  /// Path to the directory containing the individual execution trace files.
  std::string TraceDirectory;

  /// \brief Default constructor.
  ///
  InputBufferAllocator(llvm::StringRef Directory)
  : TraceDirectory(Directory)
  {}
  
  /// \brief Get a buffer for an arbitrary file.
  ///
  seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getBuffer(llvm::StringRef Path) const;
  
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
  
  /// @} (Constructors.)
  
  
  /// \brief Get the path of the directory containing the trace files.
  ///
  std::string const &getTraceDirectory() const {
    return TraceDirectory;
  }
  
  /// \brief Get the original, uninstrumented Module.
  ///
  seec::Maybe<llvm::Module *, seec::Error>
  getModule(llvm::LLVMContext &Context) const;
  
  /// \brief Get the original, uninstrumented Module's file.
  ///
  seec::Maybe<TraceFile, seec::Error> getModuleFile() const;

  /// \brief Create a MemoryBuffer containing the process data for the given
  /// Segment.
  ///
  seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getProcessData(ProcessSegment Segment) const;
  
  /// \brief Get the given process segment's file.
  ///
  seec::Maybe<TraceFile, seec::Error>
  getProcessFile(ProcessSegment Segment) const;

  /// \brief Create a MemoryBuffer containing the data for the given thread's
  /// given Segment.
  ///
  seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getThreadData(uint32_t ThreadID, ThreadSegment Segment) const;
  
  /// \brief Get the given thread segment's file.
  ///
  seec::Maybe<TraceFile, seec::Error>
  getThreadFile(uint32_t ThreadID, ThreadSegment Segment) const;
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTORAGE_HPP
