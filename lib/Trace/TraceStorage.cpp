#include "seec/Trace/TraceStorage.hpp"

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// OutputStreamAllocator
//------------------------------------------------------------------------------

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getProcessStream(llvm::StringRef Segment) {
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

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getThreadStream(uint32_t ThreadID,
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


//------------------------------------------------------------------------------
// InputBufferAllocator
//------------------------------------------------------------------------------

std::unique_ptr<llvm::MemoryBuffer>
InputBufferAllocator::getProcessData(llvm::StringRef Segment) {
  auto Path = TraceDirectory;
  Path.appendComponent("st.p");
  Path.appendSuffix(Segment);

  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

  auto Error = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (Error != llvm::error_code::success()) {
    llvm::errs() << "\nFatal error: " << Error.message() << "\n"
                 << "While opening '" << Path.str() << "'\n";
    exit(EXIT_FAILURE);
  }

  return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
}

std::unique_ptr<llvm::MemoryBuffer>
InputBufferAllocator::getThreadData(uint32_t ThreadID,
                                    llvm::StringRef Segment) {
  std::string Filename;
  llvm::raw_string_ostream FilenameStream (Filename);
  FilenameStream << "st.t" << ThreadID << "." << Segment;
  FilenameStream.flush();

  auto Path = TraceDirectory;
  Path.appendComponent(Filename);

  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

  auto Error = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (Error != llvm::error_code::success()) {
    llvm::errs() << "\nFatal error: " << Error.message() << "\n"
                 << "While opening '" << Path.str() << "'\n";
    exit(EXIT_FAILURE);
  }

  return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
}


} // namespace trace (in seec)

} // namespace seec
