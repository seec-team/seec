//===- include/seec/Trace/TraceStorage.hpp -------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_TRACESTORAGE_HPP
#define SEEC_TRACE_TRACESTORAGE_HPP

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"

#include <memory>

namespace seec {

namespace trace {

/// \brief Allocates raw_ostreams for the various outputs required by tracing.
///
/// This gives us a central area to control the output locations and filenames.
class OutputStreamAllocator {
public:
  OutputStreamAllocator() {}
  
  /// Get an output for a process-level data segment.
  std::unique_ptr<llvm::raw_ostream> getProcessStream(llvm::StringRef Segment) {
    std::string ErrorInfo;
    
    std::string Filename ("st.p.");
    Filename += Segment.str();
    
    auto Out = new llvm::raw_fd_ostream(Filename.c_str(), ErrorInfo,
                                        llvm::raw_fd_ostream::F_Binary);
    if (!Out) {
      llvm::errs() << "\nFatal error: " << ErrorInfo << "\n";
      exit(EXIT_FAILURE);
    }
    
    return std::unique_ptr<llvm::raw_ostream>(Out);
  }
  
  /// Get an output for a thread-specific data segment.
  std::unique_ptr<llvm::raw_ostream> getThreadStream(uint32_t ThreadID,
                                                     llvm::StringRef Segment) {
    std::string ErrorInfo;
    
    std::string Filename;
    llvm::raw_string_ostream FilenameStream (Filename);
    
    FilenameStream << "st.t" << ThreadID << "." << Segment;
    FilenameStream.flush();
    
    auto Out = new llvm::raw_fd_ostream(Filename.c_str(), ErrorInfo,
                                        llvm::raw_fd_ostream::F_Binary);
    if (!Out) {
      llvm::errs() << "\nFatal error: " << ErrorInfo << "\n";
      exit(EXIT_FAILURE);
    }
    
    return std::unique_ptr<llvm::raw_ostream>(Out);
  }
};

/// Gets MemoryBuffers for the various sections of a trace.
class InputBufferAllocator {

public:
  /// Constructor.
  InputBufferAllocator() {}
  
  std::unique_ptr<llvm::MemoryBuffer> getProcessData(llvm::StringRef Segment) {
    std::string Filename ("st.p.");
    Filename += Segment.str();
    
    llvm::OwningPtr<llvm::MemoryBuffer> Buffer;
    
    auto Error = llvm::MemoryBuffer::getFile(Filename, Buffer, -1, false);
    if (Error != llvm::error_code::success()) {
      llvm::errs() << "\nFatal error: " << Error.message() << "\n"
                   << "While opening '" << Filename << "'\n";
      exit(EXIT_FAILURE);
    }
    
    return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
  }
  
  std::unique_ptr<llvm::MemoryBuffer> getThreadData(uint32_t ThreadID,
                                                    llvm::StringRef Segment) {
    std::string Filename;
    llvm::raw_string_ostream FilenameStream (Filename);
    
    FilenameStream << "st.t" << ThreadID << "." << Segment;
    FilenameStream.flush();
    
    llvm::OwningPtr<llvm::MemoryBuffer> Buffer;
    
    auto Error = llvm::MemoryBuffer::getFile(Filename, Buffer, -1, false);
    if (Error != llvm::error_code::success()) {
      llvm::errs() << "\nFatal error: " << Error.message() << "\n"
                   << "While opening '" << Filename << "'\n";
      exit(EXIT_FAILURE);
    }
    
    return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
  }
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTORAGE_HPP
