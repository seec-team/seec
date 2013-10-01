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

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/system_error.h"


#include <string>

#include <unistd.h>


namespace seec {

namespace trace {


static char const *getTraceDirectoryExtension() {
  return "seec";
}

static char const *getModuleFilename() {
  return "module.bc";
}

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

static std::string getPIDString() {
  std::string PIDString;
  
  {
    llvm::raw_string_ostream Stream(PIDString);
    Stream << getpid();
    Stream.flush();
  }
  
  return PIDString;
}

//------------------------------------------------------------------------------
// OutputStreamAllocator
//------------------------------------------------------------------------------

OutputStreamAllocator::OutputStreamAllocator(llvm::StringRef Directory)
: TraceDirectory(Directory)
{}

seec::Maybe<std::unique_ptr<OutputStreamAllocator>, seec::Error>
OutputStreamAllocator::createOutputStreamAllocator()
{
  return createOutputStreamAllocator("p");
}

seec::Maybe<std::unique_ptr<OutputStreamAllocator>, seec::Error>
OutputStreamAllocator::createOutputStreamAllocator(llvm::StringRef Identifier)
{
  llvm::error_code ErrCode;
  llvm::SmallString<32> Path;
  
  // Use the current working directory if possible.
  ErrCode = llvm::sys::fs::current_path(Path);
  if (ErrCode != llvm::errc::success) {
    // Otherwise use the system's temp directory.
    llvm::sys::path::system_temp_directory(true, Path);
  }
  
  // Check if the path is a directory.
  bool IsDirectory;
  ErrCode = llvm::sys::fs::is_directory(llvm::StringRef(Path), IsDirectory);
  if (ErrCode != llvm::errc::success) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "IsDirectoryFail"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  }
  
  if (!IsDirectory)
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "PathIsNotDirectory"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  
  // Create a path to the trace directory.
  if (auto const UserPath = std::getenv("SEEC_TRACE_NAME")) {
    llvm::sys::path::append(Path, std::string{UserPath} + "." +
                                  getTraceDirectoryExtension());
  }
  else {
    std::string TraceName;
    
    for (auto const Char : Identifier)
      if (std::isalnum(Char))
        TraceName.push_back(Char);
    
    if (TraceName.empty())
      TraceName.push_back('p');
    TraceName.push_back('.');
    TraceName += getPIDString();
    TraceName.push_back('.');
    TraceName += getTraceDirectoryExtension();
    
    llvm::sys::path::append(Path, TraceName);
  }
  
  // Create the trace directory, but fail if it already exists.
  bool OutDirExisted;
  ErrCode = llvm::sys::fs::create_directory(llvm::StringRef(Path),
                                            OutDirExisted);
  if (ErrCode != llvm::errc::success) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "CreateDirectoryFail"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  }
  
  if (OutDirExisted) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "OutDirectoryExists"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  }
  
  // Create the OutputStreamAllocator.
  auto Allocator = new (std::nothrow)
                   OutputStreamAllocator(llvm::StringRef(Path));
  
  if (Allocator == nullptr)
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "OutputStreamAllocatorFail"}));
  
  return std::unique_ptr<OutputStreamAllocator>(Allocator);
}

seec::Maybe<seec::Error>
OutputStreamAllocator::writeModule(llvm::StringRef Bitcode) {
  // Get a path for the bitcode file.
  llvm::SmallString<256> Path {llvm::StringRef{TraceDirectory}};
  llvm::sys::path::append(Path, getModuleFilename());
  
  // Attempt to open an output buffer for the bitcode file.
  llvm::OwningPtr<llvm::FileOutputBuffer> Output;
  
  auto ErrCode = llvm::FileOutputBuffer::create(llvm::StringRef(Path),
                                                Bitcode.size(),
                                                Output);
  if (ErrCode != llvm::errc::success) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "FileOutputBufferFail"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  }
  
  // Copy the bitcode into the output buffer and commit it.
  std::memcpy(Output->getBufferStart(), Bitcode.data(), Bitcode.size());
  Output->commit();
  
  return seec::Maybe<seec::Error>();
}

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getProcessStream(ProcessSegment Segment, unsigned Flags)
{
  llvm::SmallString<256> Filename {llvm::StringRef(TraceDirectory)};
  
  llvm::sys::path::append(Filename, llvm::Twine("st.") +
                                    getProcessExtension(Segment));
  
  Flags |= llvm::raw_fd_ostream::F_Binary;
  
  std::string ErrorInfo;
  auto Out = new llvm::raw_fd_ostream(Filename.c_str(), ErrorInfo, Flags);
  if (!Out) {
    llvm::errs() << "\nFatal error: " << ErrorInfo << "\n";
    exit(EXIT_FAILURE);
  }

  return std::unique_ptr<llvm::raw_ostream>(Out);
}

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getThreadStream(uint32_t ThreadID,
                                       ThreadSegment Segment,
                                       unsigned Flags)
{
  std::string File;
  
  {
    llvm::raw_string_ostream FileStream {File};
    FileStream << "st.t" << ThreadID
               << "." << getThreadExtension(Segment);
    FileStream.flush();
  }
  
  llvm::SmallString<256> Path {llvm::StringRef(TraceDirectory)};
  llvm::sys::path::append(Path, File);
  
  Flags |= llvm::raw_fd_ostream::F_Binary;

  std::string ErrorInfo;
  auto Out = new llvm::raw_fd_ostream(Path.c_str(), ErrorInfo, Flags);
  if (!Out) {
    llvm::errs() << "\nFatal error: " << ErrorInfo << "\n";
    exit(EXIT_FAILURE);
  }

  return std::unique_ptr<llvm::raw_ostream>(Out);
}


//------------------------------------------------------------------------------
// InputBufferAllocator
//------------------------------------------------------------------------------

seec::Maybe<InputBufferAllocator, seec::Error>
InputBufferAllocator::createFor(llvm::StringRef Directory) {
  llvm::error_code ErrCode;
  llvm::SmallString<256> Path;
  
  if (!Directory.empty()) {
    Path = Directory;
  }
  else {
    ErrCode = llvm::sys::fs::current_path(Path);
    if (ErrCode != llvm::errc::success) {
      return Error(LazyMessageByRef::create("Trace",
                                            {"errors", "CurrentPathFail"}));
    }
  }
  
  bool IsDirectory;
  ErrCode = llvm::sys::fs::is_directory(llvm::StringRef(Path), IsDirectory);
  if (ErrCode != llvm::errc::success) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "IsDirectoryFail"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  }
  
  if (!IsDirectory) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "PathIsNotDirectory"},
                                          std::make_pair("path",
                                                         Path.c_str())));
  }
  
  return InputBufferAllocator{llvm::StringRef(Path)};
}

seec::Maybe<llvm::Module *, seec::Error>
InputBufferAllocator::getModule(llvm::LLVMContext &Context) {
  // Get the path to the bitcode file.
  llvm::SmallString<256> Path {TraceDirectory};
  llvm::sys::path::append(Path, getModuleFilename());
  
  // Create a MemoryBuffer for the file.
  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;
  
  auto ReadError = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (ReadError != llvm::error_code::success()) {
    auto Message = UnicodeString::fromUTF8(ReadError.message());
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "InputBufferAllocationFail"},
                                          std::make_pair("file", Path.c_str()),
                                          std::make_pair("error",
                                                         std::move(Message))));
  }
  
  // Parse the Module from the bitcode.
  std::string ParseError;
  auto Mod = llvm::ParseBitcodeFile(Buffer.take(), Context, &ParseError);
  
  if (!Mod) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "ParseBitcodeFileFail"},
                                          std::make_pair("error",
                                                         ParseError.c_str())));
  }
  
  return Mod;
}

seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getProcessData(ProcessSegment Segment) {
  // Get the path to the file.
  llvm::SmallString<256> Path {TraceDirectory};
  llvm::sys::path::append(Path, llvm::Twine("st.")
                                + getProcessExtension(Segment));

  // Create a MemoryBuffer for the file.
  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

  auto ReadError = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (ReadError != llvm::error_code::success()) {
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

seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getThreadData(uint32_t ThreadID, ThreadSegment Segment) {
  // Get the name of the file.
  std::string Filename;
  
  {
    llvm::raw_string_ostream FilenameStream (Filename);
    FilenameStream << "st.t" << ThreadID
                   << "." << getThreadExtension(Segment);
    FilenameStream.flush();
  }
  
  // Get the path to the file.
  llvm::SmallString<256> Path {TraceDirectory};
  llvm::sys::path::append(Path, Filename);
  
  // Create a MemoryBuffer for the file.
  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;

  auto ReadError = llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  if (ReadError != llvm::error_code::success()) {
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
