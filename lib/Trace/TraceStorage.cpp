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
#include "seec/Util/ScopeExit.hpp"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/system_error.h"

#include <wx/longlong.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <string>

#ifdef __unix__
#include <unistd.h>
#endif


namespace seec {

namespace trace {


static char const *getArchiveExtension()        { return "seec"; }
static char const *getTraceDirectoryExtension() { return "seecd"; }
static char const *getModuleFilename()          { return "module.bc"; }

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

OutputStreamAllocator::
OutputStreamAllocator(llvm::StringRef WithTraceLocation,
                      llvm::StringRef WithTraceDirectoryName,
                      llvm::StringRef WithTraceDirectoryPath,
                      llvm::StringRef WithTraceArchiveName)
: TraceLocation(WithTraceLocation),
  TraceDirectoryName(WithTraceDirectoryName),
  TraceDirectoryPath(WithTraceDirectoryPath),
  TraceArchiveName(WithTraceArchiveName),
  TraceFiles()
{}

bool OutputStreamAllocator::deleteAll()
{
  bool Result = true;
  
  wxString FullPath {TraceDirectoryPath};
  FullPath += wxFileName::GetPathSeparator();
  auto const FullPathDirLength = FullPath.size();
  
  for (auto const &File : TraceFiles) {
    // Generate the complete path.
    FullPath.Truncate(FullPathDirLength);
    FullPath += File;
    Result = Result && wxRemoveFile(FullPath);
  }
  
  Result = Result && wxRmdir(TraceDirectoryPath);
  
  return Result;
}

/// \brief Get the default location to store traces.
///
static void getDefaultTraceLocation(llvm::SmallVectorImpl<char> &Result)
{
  if (llvm::sys::fs::current_path(Result) == llvm::errc::success)
    return;
  
  llvm::sys::path::system_temp_directory(true, Result);
}

/// \brief Check to see if the given path is a valid trace location.
///
static seec::Maybe<seec::Error> checkTraceLocation(llvm::StringRef Path)
{
  bool IsDirectory;
  auto const ErrCode = llvm::sys::fs::is_directory(Path, IsDirectory);
  
  if (ErrCode != llvm::errc::success)
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "IsDirectoryFail"},
                               std::make_pair("path", Path.str().c_str()))};
  
  if (!IsDirectory)
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "PathIsNotDirectory"},
                               std::make_pair("path", Path.str().c_str()))};
  
  return seec::Maybe<seec::Error>();
}

/// \brief Construct a new trace directory.
/// \return An Error if the directory already existed.
///
static seec::Maybe<seec::Error> createTraceDirectory(llvm::StringRef Path)
{
  bool OutDirExisted;
  auto const ErrCode = llvm::sys::fs::create_directory(Path, OutDirExisted);
  
  if (ErrCode != llvm::errc::success)
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "CreateDirectoryFail"},
                               std::make_pair("path", Path.str().c_str()))};
  
  if (OutDirExisted)
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "OutDirectoryExists"},
                               std::make_pair("path", Path.str().c_str()))};
  
  return seec::Maybe<seec::Error>();
}

seec::Maybe<std::unique_ptr<OutputStreamAllocator>, seec::Error>
OutputStreamAllocator::createOutputStreamAllocator()
{
  llvm::SmallString<256> TraceLocation;
  llvm::SmallString<256> TraceDirectoryName;
  llvm::SmallString<256> TraceDirectoryPath;
  llvm::SmallString<256> TraceArchiveName;
  
  if (auto const UserPathEV = std::getenv("SEEC_TRACE_NAME")) {
    if (llvm::sys::path::is_absolute(UserPathEV)) {
      // Set the location to the directory portion of SEEC_TRACE_NAME.
      TraceLocation = UserPathEV;
      llvm::sys::path::remove_filename(TraceLocation);
    }
    else {
      getDefaultTraceLocation(TraceLocation);
    }
    
    if (llvm::sys::path::has_filename(UserPathEV)) {
      TraceDirectoryName =
        TraceArchiveName = llvm::sys::path::filename(UserPathEV);
      
      TraceDirectoryName += '.';
      TraceDirectoryName += getTraceDirectoryExtension();
      
      TraceArchiveName += '.';
      TraceArchiveName += getArchiveExtension();
    }
  }
  else {
    getDefaultTraceLocation(TraceLocation);
  }
  
  // Check that the trace location is OK.
  auto MaybeTraceLocationError = checkTraceLocation(TraceLocation);
  if (MaybeTraceLocationError.assigned<seec::Error>())
    return MaybeTraceLocationError.move<seec::Error>();
  
  // Generate a name for the trace directory if the user didn't supply one.
  if (TraceDirectoryName.empty()) {
    TraceDirectoryName.append("p.");
    TraceDirectoryName.append(std::to_string(getpid()));
    TraceDirectoryName.push_back('.');
    TraceDirectoryName.append(getTraceDirectoryExtension());
  }
  
  // Attempt to setup the trace directory.
  TraceDirectoryPath = TraceLocation;
  llvm::sys::path::append(TraceDirectoryPath, TraceDirectoryName.str());
  
  auto MaybeCreateDirError = createTraceDirectory(TraceDirectoryPath);
  if (MaybeCreateDirError.assigned<seec::Error>())
    return MaybeCreateDirError.move<seec::Error>();
  
  // Create the OutputStreamAllocator.
  auto Allocator = new (std::nothrow)
                   OutputStreamAllocator(TraceLocation,
                                         TraceDirectoryName,
                                         TraceDirectoryPath,
                                         TraceArchiveName);
  
  if (Allocator == nullptr)
    return Error{LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "OutputStreamAllocatorFail"})};
  
  return std::unique_ptr<OutputStreamAllocator>(Allocator);
}

seec::Maybe<uint64_t, seec::Error>
OutputStreamAllocator::getTotalSize() const
{
  uint64_t TotalSize = 0;
  
  wxString FullPath {TraceDirectoryPath};
  FullPath += wxFileName::GetPathSeparator();
  auto const FullPathDirLength = FullPath.size();
  
  for (auto const &File : TraceFiles) {
    // Generate the complete path.
    FullPath.Truncate(FullPathDirLength);
    FullPath += File;
    
    auto const Size = wxFileName{FullPath}.GetSize();
    if (Size == wxInvalidSize) {
      return Error{
        LazyMessageByRef::create("Trace", {"errors", "GetFileSizeFail"},
                                 std::make_pair("path", File.c_str()))};
    }
    
    TotalSize += Size.GetValue();
  }
  
  return TotalSize;
}

seec::Maybe<seec::Error>
OutputStreamAllocator::writeModule(llvm::StringRef Bitcode)
{
  // Get a path for the bitcode file.
  llvm::SmallString<256> Path {llvm::StringRef{TraceDirectoryPath}};
  llvm::sys::path::append(Path, getModuleFilename());
  
  // Attempt to open an output buffer for the bitcode file.
  llvm::OwningPtr<llvm::FileOutputBuffer> Output;
  
  auto ErrCode = llvm::FileOutputBuffer::create(llvm::StringRef(Path),
                                                Bitcode.size(),
                                                Output);
  if (ErrCode != llvm::errc::success) {
    return Error{LazyMessageByRef::create("Trace",
                                          {"errors", "FileOutputBufferFail"},
                                          std::make_pair("path",
                                                         Path.c_str()))};
  }
  
  // Copy the bitcode into the output buffer and commit it.
  std::memcpy(Output->getBufferStart(), Bitcode.data(), Bitcode.size());
  Output->commit();
  
  // Save the relative path to the module.
  TraceFiles.emplace_back(getModuleFilename());
  
  return seec::Maybe<seec::Error>();
}

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getProcessStream(ProcessSegment Segment, unsigned Flags)
{
  std::string File = std::string{"st."} + getProcessExtension(Segment);
  
  llvm::SmallString<256> Path {llvm::StringRef(TraceDirectoryPath)};
  llvm::sys::path::append(Path, File);
  
  Flags |= llvm::raw_fd_ostream::F_Binary;
  
  std::string ErrorInfo;
  auto Out = new llvm::raw_fd_ostream(Path.c_str(), ErrorInfo, Flags);
  if (!Out) {
    llvm::errs() << "\nSeeC encountered a fatal error: " << ErrorInfo << "\n";
    exit(EXIT_FAILURE);
  }

  // Save the relative path to the process segment.
  TraceFiles.emplace_back(File);
  
  return std::unique_ptr<llvm::raw_ostream>(Out);
}

std::unique_ptr<llvm::raw_ostream>
OutputStreamAllocator::getThreadStream(uint32_t ThreadID,
                                       ThreadSegment Segment,
                                       unsigned Flags)
{
  std::string File = std::string{"st.t"} + std::to_string(ThreadID)
                   + "." + getThreadExtension(Segment);
  
  llvm::SmallString<256> Path {llvm::StringRef(TraceDirectoryPath)};
  llvm::sys::path::append(Path, File);
  
  Flags |= llvm::raw_fd_ostream::F_Binary;

  std::string ErrorInfo;
  auto Out = new llvm::raw_fd_ostream(Path.c_str(), ErrorInfo, Flags);
  if (!Out) {
    llvm::errs() << "\nSeeC encountered a fatal error: " << ErrorInfo << "\n";
    exit(EXIT_FAILURE);
  }
  
  // Save the relative path to the thread segment.
  TraceFiles.emplace_back(File);

  return std::unique_ptr<llvm::raw_ostream>(Out);
}

wxString getArchivePath(std::string const &TraceLocation,
                        std::string const &TraceDirectoryName,
                        std::string const &TraceArchiveName,
                        llvm::StringRef GivenPath)
{
  // User-supplied archive name, with extension added previously.
  if (!TraceArchiveName.empty())
    return wxString{TraceLocation} + wxFileName::GetPathSeparator()
           + TraceArchiveName;
  
  // Path given by the client (e.g. the executable name).
  if (!GivenPath.empty())
    return wxString{TraceLocation} + wxFileName::GetPathSeparator()
           + GivenPath.str() + "." + getArchiveExtension();
  
  // Path based on the trace directory name.
  wxFileName ArchiveName {TraceDirectoryName};
  ArchiveName.SetExt(getArchiveExtension());
  return wxString{TraceLocation} + wxFileName::GetPathSeparator()
         + ArchiveName.GetFullPath();
}

seec::Maybe<seec::Error> OutputStreamAllocator::archiveTo(llvm::StringRef Path)
{
  wxString const ArchivePath =
    getArchivePath(TraceLocation, TraceDirectoryName, TraceArchiveName, Path);
  
  wxFileOutputStream RawOutput{ArchivePath};
  if (!RawOutput.IsOk())
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "CreateArchiveFail"},
                               std::make_pair("path", ArchivePath.c_str()))};
  
  // If we exit early because the archive creation failed, delete the file.
  auto DeleteFailedArchive = seec::scopeExit([&]()->void{
    wxRemoveFile(ArchivePath);
  });
  
  wxZipOutputStream Output{RawOutput};
  if (!Output.IsOk())
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "CreateArchiveFail"},
                               std::make_pair("path", ArchivePath.c_str()))};
  
  // Write all trace files into the archive.
  Output.PutNextDirEntry("trace");
  
  wxString FullPath {TraceDirectoryPath};
  FullPath += wxFileName::GetPathSeparator();
  auto const FullPathDirLength = FullPath.size();
  
  for (auto const &File : TraceFiles) {
    // Generate the complete path.
    FullPath.Truncate(FullPathDirLength);
    FullPath += File;
    
    // Attempt to open the file for reading.
    wxFFileInputStream Input{FullPath};
    if (!Input.IsOk())
      return Error{
        LazyMessageByRef::create("Trace", {"errors", "ReadFileForArchiveFail"},
                                 std::make_pair("path", FullPath.c_str()))};
    
    // Create the archive entry.
    Output.PutNextEntry(wxString{"trace/"} + File);
    Output.Write(Input);
  }
  
  if (!Output.Close())
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "CreateArchiveFail"},
                               std::make_pair("path", ArchivePath.c_str()))};
  
  // The archive creation was successful, so don't delete it when we exit.
  DeleteFailedArchive.disable();
  
  // Attempt to delete the uncompressed trace files and directory.
  if (!deleteAll())
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "DeleteFilesFail"})};
  
  // The unassigned Maybe indicates that everything was OK.
  return seec::Maybe<seec::Error>();
}


//------------------------------------------------------------------------------
// InputBufferAllocator
//------------------------------------------------------------------------------

seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getBuffer(llvm::StringRef Path) const
{
  llvm::OwningPtr<llvm::MemoryBuffer> Buffer;
  
  auto const ReadError =
    llvm::MemoryBuffer::getFile(Path.str(), Buffer, -1, false);
  
  if (ReadError != llvm::error_code::success()) {
    auto Message = UnicodeString::fromUTF8(ReadError.message());
    
    return Error(
      LazyMessageByRef::create("Trace",
                               {"errors", "InputBufferAllocationFail"},
                                std::make_pair("file", Path.str().c_str()),
                                std::make_pair("error", std::move(Message))));
  }
  
  return std::unique_ptr<llvm::MemoryBuffer>(Buffer.take());
}

seec::Maybe<InputBufferAllocator, seec::Error>
InputBufferAllocator::createFor(llvm::StringRef Directory)
{
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
InputBufferAllocator::getModule(llvm::LLVMContext &Context) const
{
  // Get the path to the bitcode file.
  llvm::SmallString<256> Path {TraceDirectory};
  llvm::sys::path::append(Path, getModuleFilename());
  
  // Create a MemoryBuffer for the file.
  auto MaybeBuffer = getBuffer(Path);
  if (MaybeBuffer.assigned<seec::Error>())
    return MaybeBuffer.move<seec::Error>();
  auto &Buffer = MaybeBuffer.get<std::unique_ptr<llvm::MemoryBuffer>>();
  
  // Parse the Module from the bitcode.
  std::string ParseError;
  auto Mod = llvm::ParseBitcodeFile(Buffer.release(), Context, &ParseError);
  
  if (!Mod) {
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "ParseBitcodeFileFail"},
                                          std::make_pair("error",
                                                         ParseError.c_str())));
  }
  
  return Mod;
}

seec::Maybe<TraceFile, seec::Error>
InputBufferAllocator::getModuleFile() const
{
  // Get the path to the bitcode file.
  llvm::SmallString<256> Path {TraceDirectory};
  llvm::sys::path::append(Path, getModuleFilename());
  
  // Create a MemoryBuffer for the file.
  auto MaybeBuffer = getBuffer(Path);
  if (MaybeBuffer.assigned<seec::Error>())
    return MaybeBuffer.move<seec::Error>();
  
  return TraceFile{getModuleFilename(),
                   MaybeBuffer.move<std::unique_ptr<llvm::MemoryBuffer>>()};
}

seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getProcessData(ProcessSegment Segment) const
{
  // Get the path to the file.
  llvm::SmallString<256> Path {TraceDirectory};
  llvm::sys::path::append(Path, llvm::Twine("st.")
                                + getProcessExtension(Segment));

  // Create a MemoryBuffer for the file.
  auto MaybeBuffer = getBuffer(Path);
  if (MaybeBuffer.assigned<seec::Error>())
    return MaybeBuffer.move<seec::Error>();

  return MaybeBuffer.move<std::unique_ptr<llvm::MemoryBuffer>>();
}

seec::Maybe<TraceFile, seec::Error>
InputBufferAllocator::getProcessFile(ProcessSegment Segment) const
{
  auto MaybeBuffer = getProcessData(Segment);
  if (MaybeBuffer.assigned<seec::Error>())
    return MaybeBuffer.move<seec::Error>();
  
  return TraceFile{std::string{"st."} + getProcessExtension(Segment),
                   MaybeBuffer.move<std::unique_ptr<llvm::MemoryBuffer>>()};
}

seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>
InputBufferAllocator::getThreadData(uint32_t ThreadID, ThreadSegment Segment)
const
{
  auto MaybeFile = getThreadFile(ThreadID, Segment);
  
  if (MaybeFile.assigned<seec::Error>())
    return MaybeFile.move<seec::Error>();
  
  if (MaybeFile.assigned<TraceFile>())
    return std::move(MaybeFile.get<TraceFile>().getContents());
  
  return seec::Maybe<std::unique_ptr<llvm::MemoryBuffer>, seec::Error>();
}

seec::Maybe<TraceFile, seec::Error>
InputBufferAllocator::getThreadFile(uint32_t ThreadID, ThreadSegment Segment)
const
{
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
  auto MaybeBuffer = getBuffer(Path);
  if (MaybeBuffer.assigned<seec::Error>())
    return MaybeBuffer.move<seec::Error>();
  
  return TraceFile{std::move(Filename),
                   MaybeBuffer.move<std::unique_ptr<llvm::MemoryBuffer>>()};
}


} // namespace trace (in seec)

} // namespace seec
