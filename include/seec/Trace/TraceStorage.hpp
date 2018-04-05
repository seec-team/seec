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

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <system_error>

#include <type_safe/flag.hpp>
#include <type_safe/strong_typedef.hpp>


namespace llvm {
  class LLVMContext;
  class Module;
}


namespace seec {

namespace trace {


class OutputBlock;
class OutputStreamAllocator;


/// \brief Offset of some data in the trace.
///
struct trace_offset
: type_safe::strong_typedef<trace_offset, uint64_t>
{
  using strong_typedef::strong_typedef;
};


///
///
class OutputBlock {
public:
  static constexpr off_t getHeaderSize() {
    // BlockType, NextBlock
    return sizeof(BlockType) + sizeof(uint64_t);
  }
  
  /// \brief Writes to a set position in the trace.
  ///
  class WriteRecord {
    int const m_TraceFD;
    
    off_t const m_Offset;
    
    size_t const m_Length;
    
  public:
    WriteRecord(int TraceFD, off_t Offset, size_t Length)
    : m_TraceFD(TraceFD),
      m_Offset(Offset),
      m_Length(Length)
    {}
    
    off_t const getOffset() const { return m_Offset; }
    
    type_safe::boolean rewrite(const void * const buf, size_t const nbyte) {
      assert(nbyte == m_Length);
      return OutputBlock::writeat(m_TraceFD, buf, nbyte, m_Offset);
    }
  };
  
  OutputBlock(int TraceFD,
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
  
  OutputBlock(OutputBlock &&Other)
  : m_TraceFD(Other.m_TraceFD),
    m_BlockEnd(Other.m_BlockEnd),
    m_Offset(Other.m_Offset.exchange(std::numeric_limits<off_t>::max()))
  {}
  
  /// If the write was successful, returns the offset at which the data was
  /// written.
  llvm::Optional<off_t> write(const void *buf, size_t nbyte);
  
  llvm::Optional<WriteRecord> rewritableWrite(const void *buf, size_t nbyte);
  
private:
  static type_safe::boolean
  writeat(int fd, const void *buf, size_t nbyte, off_t offset);

  int const m_TraceFD;
  
  off_t const m_BlockEnd;
  
  std::atomic<off_t> m_Offset;
};


/// \brief
/// Accumulates output in a buffer, then commits it to the
/// \c OutputStreamAllocator in a single block.
///
class OutputBlockBuilder {
public:
  struct relative_offset
  : type_safe::strong_typedef<relative_offset, uint64_t>
  {
    using strong_typedef::strong_typedef;
  };
  
  struct block_offset
  : type_safe::strong_typedef<block_offset, uint64_t>
  {
    using strong_typedef::strong_typedef;
  };
  
  OutputBlockBuilder(OutputStreamAllocator &Output,
                     BlockType Type)
  : m_Output(Output),
    m_BlockType(Type),
    m_Written(false),
    m_Buffer(),
    m_OStream(m_Buffer)
  {}
  
  ~OutputBlockBuilder()
  {
    write();
  }
  
  llvm::raw_svector_ostream &getOStream() {
    return m_OStream;
  }
  
  relative_offset getCurrentOffset() {
    return relative_offset(m_Buffer.size());
  }
  
  /// Create and write an output block for a builder.
  /// The builder is consumed.
  ///
  static llvm::Optional<block_offset>
  flush(std::unique_ptr<OutputBlockBuilder> Builder);
  
private:
  OutputStreamAllocator &m_Output;
  
  BlockType m_BlockType;
  
  type_safe::flag m_Written;
  
  llvm::SmallVector<char, 256> m_Buffer;
  
  llvm::raw_svector_ostream m_OStream;
  
  llvm::Optional<block_offset> write();
};

inline trace_offset operator+(OutputBlockBuilder::block_offset const &L,
                              OutputBlockBuilder::relative_offset const &R)
{
  return trace_offset(static_cast<uint64_t>(L) + static_cast<uint64_t>(R));
}


/// \brief
/// This class is not internally thread-safe.
///
class OutputBlockStream {
public:
  using HeaderWriterFnTy = void (OutputBlock &);
  
  OutputBlockStream(OutputStreamAllocator &Output,
                    BlockType Type,
                    off_t BlockSize)
  : m_Output(Output),
    m_BlockType(Type),
    m_BlockSize(BlockSize),
    m_Block(),
    m_HeaderWriter()
  {}
  
  OutputBlockStream(OutputStreamAllocator &Output,
                    BlockType Type,
                    off_t BlockSize,
                    std::function<HeaderWriterFnTy> HeaderWriter)
  : OutputBlockStream(Output, Type, BlockSize)
  {
    m_HeaderWriter = HeaderWriter;
  }
  
  llvm::Optional<off_t> write(void const *Data, size_t Size);
  
  llvm::Optional<OutputBlock::WriteRecord>
  rewritableWrite(const void *Data, size_t Size);

private:
  void getNewBlock();
  
  OutputStreamAllocator &m_Output;
  
  BlockType const m_BlockType;
  
  off_t const m_BlockSize;
  
  llvm::Optional<OutputBlock> m_Block;
  
  std::function<HeaderWriterFnTy> m_HeaderWriter;
};


/// \brief 
/// This class is synchronized.
///
class OutputBlockProcessDataStream {
public:
  OutputBlockProcessDataStream(OutputStreamAllocator &Output)
  : m_Output(Output),
    m_OutputStream(Output, BlockType::ProcessData, getBlockSize())
  {}
  
  llvm::Optional<off_t> write(void const *Data, size_t Size);
  
private:
  /// The size of standard data blocks.
  static constexpr off_t getBlockSize() { return 4096; }
  
  /// If a single write is larger than this, use a separate individual block
  /// to store the write.
  static constexpr size_t getSingleBlockThreshold() { return 1024; }
  
  OutputStreamAllocator &m_Output;
  
  OutputBlockStream m_OutputStream;
};


/// \brief
/// This class is not internally thread-safe.
///
class OutputBlockThreadEventStream {
public:
  OutputBlockThreadEventStream(OutputStreamAllocator &Output,
                               uint32_t ThreadID)
  : m_ThreadID(ThreadID),
    m_OutputStream(Output, BlockType::ThreadEvents, getBlockSize(),
                   [this] (OutputBlock &B) { this->writeHeader(B); })
  {}
  
  llvm::Optional<off_t> write(void const * const Data, size_t const Size) {
    return m_OutputStream.write(Data, Size);
  }
  
  llvm::Optional<OutputBlock::WriteRecord>
  rewritableWrite(const void * const Data, size_t const Size) {
    return m_OutputStream.rewritableWrite(Data, Size);
  }
  
private:
  static constexpr off_t getBlockSize() { return 4096; }
  
  void writeHeader(OutputBlock &Block);
  
  uint32_t m_ThreadID;
  
  OutputBlockStream m_OutputStream;
};


/// \brief Allocates raw_ostreams for the various outputs required by tracing.
///
/// This gives us a central area to control the output locations and filenames.
///
class OutputStreamAllocator {
  /// The path to the trace file.
  std::string m_TracePath;
  
  /// Was the trace filename specified by the user?
  bool m_UserSpecifiedTraceName;
  
  /// The root file descriptor for the trace file.
  int m_TraceFD;
  
  /// 
  std::atomic<off_t> m_TraceOffset;
  
  /// \brief Create a new OutputStreamAllocator.
  ///
  OutputStreamAllocator(llvm::StringRef WithTraceName,
                        bool UserSpecifiedTraceName,
                        int TraceFD);
  
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
  
  /// \brief Get the size of the trace file (in bytes).
  ///
  uint64_t getTotalSize() const;
  
  /// @} (Accessors.)
  
  
  /// \name Mutators.
  /// @{
  
  /// \brief Rename the trace file based on the program name.
  ///
  void updateTraceName(llvm::StringRef ProgramName);
  
  /// \brief Create a new output block in the trace file.
  ///
  llvm::Optional<OutputBlock> getOutputBlock(BlockType Type, off_t NBytes);
  
  /// \brief Write the Module's bitcode to the trace.
  ///
  seec::Maybe<seec::Error> writeModule(llvm::StringRef Bitcode);
  
  /// \brief Get output for process-level trace data.
  ///
  std::unique_ptr<OutputBlockBuilder> getProcessTraceStream();
  
  /// \brief Get output for process-level data.
  ///
  std::unique_ptr<OutputBlockProcessDataStream> getProcessDataStream();
  
  /// \brief Get an output for a thread's Event stream.
  ///
  std::unique_ptr<OutputBlockThreadEventStream>
  getThreadEventStream(uint32_t ThreadID);

  /// @} (Mutators.)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESTORAGE_HPP
