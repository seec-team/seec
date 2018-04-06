//===----------------------------------------------------------------------===//
///
/// \file
///
//===----------------------------------------------------------------------===//

#include "seec/Trace/TraceStorage.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <system_error>

#if defined(__unix__)
  #include <unistd.h>
#elif (defined(__APPLE__) && defined(__MACH__))
  #include <sys/types.h>
  #include <sys/uio.h>
  #include <unistd.h>
#elif defined(_WIN32)
  #include <process.h>
  #include <windows.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>


namespace seec {

namespace trace {


static char const *getTraceExtension() { return "seec"; }

// MinGW doesn't implement pwrite. This is a simple workaround for SeeC, noting
// that we never mix write() and pwrite() on the same file descriptor (this is
// important because this workaround for pwrite() modifies the file pointer,
// but the real pwrite() does not).
//   See: https://gist.github.com/przemoc/fbf2bfb11af0d9cd58726c200e4d133e
//        https://sourceforge.net/p/mingw/mailman/message/35742927/
#if defined(__MINGW32__)
namespace {

ssize_t pwrite(int fd, const void *buf, size_t count, long long offset)
{
  OVERLAPPED o = {0,0,0,0,0};
  HANDLE fh = (HANDLE)_get_osfhandle(fd);
  uint64_t off = offset;
  DWORD bytes;
  BOOL ret;

  if (fh == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return -1;
  }

  o.Offset = off & 0xffffffff;
  o.OffsetHigh = (off >> 32) & 0xffffffff;
  
  ret = WriteFile(fh, buf, (DWORD)count, &bytes, &o);
  if (!ret) {
    errno = EIO;
    return -1;
  }
  
  return (ssize_t)bytes;
}

}
#endif

//------------------------------------------------------------------------------
// OutputBlock
//------------------------------------------------------------------------------

OutputBlock::OutputBlock(int TraceFD,
                         BlockType Type,
                         off_t BlockEnd,
                         off_t Offset)
: m_TraceFD(TraceFD),
  m_BlockEnd(BlockEnd),
  m_Offset(Offset)
{
  write(&Type, sizeof(Type));
  
  uint64_t NextBlock = BlockEnd;
  write(&NextBlock, sizeof(NextBlock));
}

llvm::Optional<off_t> OutputBlock::write(const void *buf, size_t nbyte)
{
  if (m_Offset < m_BlockEnd) {
    off_t const DataOffset = m_Offset.fetch_add(nbyte);
    
    assert(nbyte <= std::numeric_limits<int64_t>::max());
    
    if (int64_t(nbyte) <= m_BlockEnd - DataOffset) {
      auto const NWritten = pwrite(m_TraceFD, buf, nbyte, DataOffset);
      if (NWritten >= 0 && uint64_t(NWritten) == nbyte) {
        return DataOffset;
      }
      else if (NWritten < 0) {
        perror("OutputBlock pwrite failed:");
      }
      else {
        perror("OutputBlock pwrite incomplete:");
      }
    }
  }
  
  return llvm::Optional<off_t>();
}

llvm::Optional<OutputBlock::WriteRecord>
OutputBlock::rewritableWrite(const void * const buf, size_t const nbyte)
{
  llvm::Optional<WriteRecord> Ret;
  
  auto Off = write(buf, nbyte);
  if (Off) {
    Ret.emplace(m_TraceFD, *Off, nbyte);
  }
  
  return Ret;
}

type_safe::boolean OutputBlock::writeat(int const fd,
                                        const void * const buf,
                                        size_t const nbyte,
                                        off_t const offset)
{
  auto const BytesWritten = pwrite(fd, buf, nbyte, offset);
  return BytesWritten >= 0 && uint64_t(BytesWritten) == nbyte;
}


//------------------------------------------------------------------------------
// OutputBlockBuilder
//------------------------------------------------------------------------------

llvm::Optional<OutputBlockBuilder::block_offset>
OutputBlockBuilder::flush(std::unique_ptr<OutputBlockBuilder> Builder)
{
  return Builder->write();
}

llvm::Optional<OutputBlockBuilder::block_offset> OutputBlockBuilder::write()
{
  llvm::Optional<OutputBlockBuilder::block_offset> Ret;
  
  if (m_Written.try_set()) {
    auto const TotalBlockSize = OutputBlock::getHeaderSize() + m_Buffer.size();
    auto Block = m_Output.getOutputBlock(m_BlockType, TotalBlockSize);
    
    if (Block) {
      auto Off = Block->write(m_Buffer.data(), m_Buffer.size());
      if (Off) {
        Ret = block_offset(*Off);
      }
      else {
        fprintf(stderr, "couldn't write to block builder basic block.\n");
      }
    }
  }
  
  return Ret;
}

//------------------------------------------------------------------------------
// OutputBlockStream
//------------------------------------------------------------------------------

llvm::Optional<off_t> OutputBlockStream::write(void const * const Data,
                                               size_t const Size)
{
  llvm::Optional<off_t> Ret;
  
  if (!m_Block) {
    getNewBlock();
  }
  
  if (m_Block) {
    Ret = m_Block->write(Data, Size);
    
    if (!Ret) {
      // Perhaps the old block was out of room, get a new one and try again.
      getNewBlock();
      Ret = m_Block->write(Data, Size);
    }
  }
  
  return Ret;
}

llvm::Optional<OutputBlock::WriteRecord>
OutputBlockStream::rewritableWrite(const void *Data, size_t Size)
{
  llvm::Optional<OutputBlock::WriteRecord> Ret;
  
  if (!m_Block) {
    getNewBlock();
  }
  
  if (m_Block) {
    if (auto Result = m_Block->rewritableWrite(Data, Size)) {
      Ret.emplace(*Result);
    }
    else {
      // Perhaps the old block was out of room, get a new one and try again.
      getNewBlock();
      if (auto Result = m_Block->rewritableWrite(Data, Size)) {
        Ret.emplace(*Result);
      }
    }
  }
  
  return Ret;
}

void OutputBlockStream::getNewBlock()
{
  m_Block.reset();
  
  auto NewBlock = m_Output.getOutputBlock(m_BlockType, m_BlockSize);
  if (NewBlock) {
    m_Block.emplace(std::move(*NewBlock));
  }

  if (m_Block && m_HeaderWriter) {
    m_HeaderWriter(*m_Block);
  }
}


//------------------------------------------------------------------------------
// OutputBlockProcessDataStream
//------------------------------------------------------------------------------

llvm::Optional<off_t>
OutputBlockProcessDataStream::write(const void * const Data,
                                    size_t const Size)
{
  llvm::Optional<off_t> Ret;
  
  if (Size < getSingleBlockThreshold()) {
    Ret = m_OutputStream.write(Data, Size);
  }
  else {
    auto const TotalSize = OutputBlock::getHeaderSize() + Size;
    auto SingleBlock = m_Output.getOutputBlock(BlockType::ProcessData,
                                               TotalSize);
    assert(SingleBlock);
    Ret = SingleBlock->write(Data, Size);
  }
  
  return Ret;
}


//------------------------------------------------------------------------------
// OutputBlockThreadEventStream
//------------------------------------------------------------------------------

void OutputBlockThreadEventStream::writeHeader(OutputBlock &Block)
{
  auto const Off = Block.write(&m_ThreadID, sizeof(m_ThreadID));
  assert(Off.hasValue() && "couldn't write thread event block header");
  
  // Prev Thread Block
  // Next Thread Block
}


//------------------------------------------------------------------------------
// OutputStreamAllocator
//------------------------------------------------------------------------------

OutputStreamAllocator::
OutputStreamAllocator(llvm::StringRef WithTracePath,
                      bool UserSpecifiedTraceName,
                      int const TraceFD)
: m_TracePath(WithTracePath),
  m_UserSpecifiedTraceName(UserSpecifiedTraceName),
  m_TraceFD(TraceFD),
  m_TraceOffset(0)
{
  // Setup the file header.
  auto const Written = write(m_TraceFD, "SEECSEEC", 8);
  if (Written < 0) {
    perror("writing magic failed:");
    exit(EXIT_FAILURE);
  }
  
  m_TraceOffset += 8;
}

bool OutputStreamAllocator::deleteAll()
{
  bool Result = true;
  
  if (close(m_TraceFD) == 0) {
    m_TraceFD = -1;
  }
  
  if (unlink(m_TracePath.c_str())) {
    Result = false;
  }
  
  return Result;
}

/// \brief Get the default location to store traces.
///
static void getDefaultTraceLocation(llvm::SmallVectorImpl<char> &Result)
{
  auto const Err = llvm::sys::fs::current_path(Result);
  if (!Err)
    return;
  
  llvm::sys::path::system_temp_directory(true, Result);
}

seec::Maybe<std::unique_ptr<OutputStreamAllocator>, seec::Error>
OutputStreamAllocator::createOutputStreamAllocator()
{
  llvm::SmallString<256> TraceLocation;
  llvm::SmallString<256> TraceFileName;
  bool UserSpecifiedTraceName = false;
  
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
      TraceFileName = llvm::sys::path::filename(UserPathEV);
      UserSpecifiedTraceName = true;
    }
  }
  else {
    getDefaultTraceLocation(TraceLocation);
  }
  
  // Generate a name for the trace file if the user didn't supply one.
  if (TraceFileName.empty()) {
    TraceFileName.append("p.");
#if defined(__unix__)
    TraceFileName.append(std::to_string(getpid()));
#elif defined(_WIN32)
	TraceFileName.append(std::to_string(_getpid()));
#endif
    TraceFileName.push_back('.');
    TraceFileName.append(getTraceExtension());
  }
  
  // Create our file.
  llvm::SmallString<256> FullPath = TraceLocation;
  llvm::sys::path::append(FullPath, TraceFileName);
  
#if defined(_WIN32)
  // We have to create the file with FILE_SHARE_DELETE so that we can rename it
  // later (if we are supplied the program name). see:
  //   https://stackoverflow.com/questions/7147577/programmatically-rename-open-
  //   file-on-windows
  HANDLE const TraceHandle =
    CreateFile(FullPath.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_DELETE, // Allow us to rename the file.
      NULL,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
  
  if (TraceHandle == INVALID_HANDLE_VALUE) {
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "OpenFileFail"},
                               std::make_pair("error", strerror(errno)))};
  }
  
  int const TraceFD = _open_osfhandle(reinterpret_cast<intptr_t>(TraceHandle),
                                      _O_CREAT | _O_WRONLY | _O_TRUNC);
#else
  mode_t const TraceMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int const TraceFD = open(FullPath.c_str(),
                           O_WRONLY | O_CREAT | O_TRUNC,
                           TraceMode);
#endif
  
  if (TraceFD == -1) {
    return Error{
      LazyMessageByRef::create("Trace", {"errors", "OpenFileFail"},
                               std::make_pair("error", strerror(errno)))};
  }
  
  // Create the OutputStreamAllocator.
  std::unique_ptr<OutputStreamAllocator> Allocator (
    new (std::nothrow) OutputStreamAllocator(FullPath,
                                             UserSpecifiedTraceName,
                                             TraceFD));
  
  if (!Allocator)
    return Error{LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "OutputStreamAllocatorFail"})};
  
  // Setup the file header.
  
  return std::move(Allocator);
}

uint64_t OutputStreamAllocator::getTotalSize() const
{
  return m_TraceOffset;
}

void OutputStreamAllocator::updateTraceName(llvm::StringRef ProgramName)
{
  // Rename the file (if it wasn't a user-supplied name).
  if (!m_UserSpecifiedTraceName) {
    llvm::SmallString<256> NewName (m_TracePath);
    llvm::sys::path::remove_filename(NewName);
    llvm::sys::path::append(NewName, ProgramName);
    llvm::sys::path::replace_extension(NewName, getTraceExtension());
    
#if defined(_WIN32)
    // Remove any existing trace with the new name. Usually rename() would do
    // this automatically, but not under msys2-mingw64.
    unlink(NewName.c_str());
#endif
    
    auto const Result = rename(m_TracePath.c_str(), NewName.c_str());
    
    if (!Result) {
      m_TracePath = NewName.str();
    }
  }
}

llvm::Optional<OutputBlock>
OutputStreamAllocator::getOutputBlock(BlockType Type, off_t NBytes)
{
  off_t const Offset = m_TraceOffset.fetch_add(NBytes);
  off_t const End = Offset + NBytes;
  return OutputBlock(m_TraceFD, Type, End, Offset);
}

seec::Maybe<seec::Error>
OutputStreamAllocator::writeModule(llvm::StringRef Bitcode)
{
  // Header
  off_t const Size = Bitcode.size();
  
  auto Output = getOutputBlock(BlockType::ModuleBitcode,
                               OutputBlock::getHeaderSize() + Size);
  if (!Output) {
    return Error(
      LazyMessageByRef::create("Trace",
                               {"errors", "OutputBlockFail"}));
  }
  
  auto Result = Output->write(Bitcode.data(), Bitcode.size());
  if (!Result) {
    return Error(
      LazyMessageByRef::create("Trace",
                               {"errors", "OutputBlockFail"}));
  }
  
  return seec::Maybe<seec::Error>();
}

std::unique_ptr<OutputBlockBuilder>
OutputStreamAllocator::getProcessTraceStream()
{
  return llvm::make_unique<OutputBlockBuilder>(*this, BlockType::ProcessTrace);
}

std::unique_ptr<OutputBlockProcessDataStream>
OutputStreamAllocator::getProcessDataStream()
{
  return llvm::make_unique<OutputBlockProcessDataStream>(*this);
}

std::unique_ptr<OutputBlockThreadEventStream>
OutputStreamAllocator::getThreadEventStream(uint32_t const ThreadID)
{
  return llvm::make_unique<OutputBlockThreadEventStream>(*this, ThreadID);
}


} // namespace trace (in seec)

} // namespace seec
