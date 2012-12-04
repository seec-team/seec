//===- lib/Trace/TraceStorage.cpp -----------------------------------------===//
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

seec::util::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getProcessData(ProcessSegment Segment) {
  auto Path = TraceDirectory;
  Path.appendComponent("st");
  Path.appendSuffix(getProcessExtension(Segment));

  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

  auto ReadError = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (ReadError != llvm::error_code::success()) {
    using namespace seec;
    auto Message = UnicodeString::fromUTF8(ReadError.message());
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "InputBufferAllocationFail"},
                                          std::make_pair("file", Path.c_str()),
                                          std::make_pair("error",
                                                         std::move(Message))));
  }

  return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
}

seec::util::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getThreadData(uint32_t ThreadID, ThreadSegment Segment) {
  std::string Filename;
  llvm::raw_string_ostream FilenameStream (Filename);
  FilenameStream << "st.t" << ThreadID
                 << "." << getThreadExtension(Segment);
  FilenameStream.flush();

  auto Path = TraceDirectory;
  Path.appendComponent(Filename);

  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

  auto ReadError = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (ReadError != llvm::error_code::success()) {
    using namespace seec;
    auto Message = UnicodeString::fromUTF8(ReadError.message());
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "InputBufferAllocationFail"},
                                          std::make_pair("file", Path.c_str()),
                                          std::make_pair("error",
                                                         std::move(Message))));
  }

  return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
}


} // namespace trace (in seec)

} // namespace seec
