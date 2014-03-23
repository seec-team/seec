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
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"

#include <map>
#include <memory>
#include <string>
#include <set>


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
  std::set<std::string> TraceFiles;
  
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
                   llvm::sys::fs::OpenFlags Flags =
                     llvm::sys::fs::OpenFlags::F_None);

  /// \brief Get an output for a thread-specific data segment.
  ///
  std::unique_ptr<llvm::raw_ostream>
  getThreadStream(uint32_t ThreadID,
                  ThreadSegment Segment,
                  llvm::sys::fs::OpenFlags Flags =
                    llvm::sys::fs::OpenFlags::F_None);
  
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

  /// Paths for temporary files (if used).
  std::vector<std::string> TempFiles;

  /// \brief Constructor (no temporaries).
  ///
  InputBufferAllocator(llvm::StringRef Directory)
  : TraceDirectory(Directory),
    TempFiles()
  {}

  /// \brief Constructor (with temporary files).
  ///
  InputBufferAllocator(std::string WithTempDirectory,
                       std::vector<std::string> WithTempFiles)
  : TraceDirectory(std::move(WithTempDirectory)),
    TempFiles(std::move(WithTempFiles))
  {}

  /// \brief Get a buffer for an arbitrary file.
  ///
  seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
  getBuffer(llvm::StringRef Path) const;

public:
  /// \brief Destructor. Deletes temporary files and directories.
  ///
  ~InputBufferAllocator();

  /// \name Constructors.
  /// @{

  // No copying.
  InputBufferAllocator(InputBufferAllocator const &) = delete;
  InputBufferAllocator &operator=(InputBufferAllocator const &) = delete;

  // Moving.
  InputBufferAllocator(InputBufferAllocator &&) = default;
  InputBufferAllocator &operator=(InputBufferAllocator &&) = default;

  /// \brief Create an \c InputBufferAllocator for a trace archive.
  /// The archive contents will be extracted to a temporary directory, and will
  /// be deleted by the destructor of the \c InputBufferAllocator.
  /// \param Path the path to the trace archive.
  /// \return The \c InputBufferAllocator or a \c seec::Error describing the
  ///         reason why it could not be created.
  ///
  static seec::Maybe<InputBufferAllocator, seec::Error>
  createForArchive(llvm::StringRef Path);

  /// \brief Create an \c InputBufferAllocator for a trace directory.
  /// \param Path the path to the trace directory.
  /// \return The \c InputBufferAllocator or a \c seec::Error describing the
  ///         reason why it could not be created.
  ///
  static seec::Maybe<InputBufferAllocator, seec::Error>
  createForDirectory(llvm::StringRef Path);

  /// \brief Attempt to create an \c InputBufferAllocator.
  /// \param Path the path to the trace archive or directory.
  /// \return The \c InputBufferAllocator or a \c seec::Error describing the
  ///         reason why it could not be created.
  ///
  static
  seec::Maybe<InputBufferAllocator, seec::Error>
  createFor(llvm::StringRef Path);

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
