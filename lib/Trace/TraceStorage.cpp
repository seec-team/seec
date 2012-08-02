#include "seec/Trace/TraceStorage.hpp"

namespace seec {

namespace trace {


static char const *getProcessExtension(ProcessSegment Segment) {
  switch (Segment) {
    case ProcessSegment::Trace:
      return "spt";
    case ProcessSegment::Data:
      return "spd";
  }
  
  return nullptr;
}

static char const *getThreadExtension(ThreadSegment Segment) {
  switch (Segment) {
    case ThreadSegment::Trace:
      return "stt";
    case ThreadSegment::Events:
      return "ste";
  }
  
  return nullptr;
}

//------------------------------------------------------------------------------
// OutputStreamAllocator
//------------------------------------------------------------------------------

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getProcessStream(ProcessSegment Segment) {
  std::string Filename ("st.");
  Filename += getProcessExtension(Segment);
  
  std::string ErrorInfo;
  auto Out = new llvm::raw_fd_ostream(Filename.c_str(),
                                      ErrorInfo,
                                      llvm::raw_fd_ostream::F_Binary);
  if (!Out) {
    llvm::errs() << "\nFatal error: " << ErrorInfo << "\n";
    exit(EXIT_FAILURE);
  }

  return std::unique_ptr<llvm::raw_ostream>(Out);
}

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getThreadStream(uint32_t ThreadID,
                                       ThreadSegment Segment) {
  std::string Filename;
  llvm::raw_string_ostream FilenameStream (Filename);
  FilenameStream << "st.t" << ThreadID
                 << "." << getThreadExtension(Segment);
  FilenameStream.flush();

  std::string ErrorInfo;
  auto Out = new llvm::raw_fd_ostream(Filename.c_str(),
                                      ErrorInfo,
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
InputBufferAllocator::getProcessData(ProcessSegment Segment) {
  auto Path = TraceDirectory;
  Path.appendComponent("st");
  Path.appendSuffix(getProcessExtension(Segment));

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
                                    ThreadSegment Segment) {
  std::string Filename;
  llvm::raw_string_ostream FilenameStream (Filename);
  FilenameStream << "st.t" << ThreadID
                 << "." << getThreadExtension(Segment);
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
