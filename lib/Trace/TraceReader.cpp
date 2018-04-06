//===- lib/Trace/TraceReader.cpp ------------------------------------------===//
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

#include "seec/RuntimeErrors/ArgumentTypes.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/Util/Serialization.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <wx/archive.h>
#include <wx/wfstream.h>

#include <cstdio>
#include <memory>
#include <numeric>
#include <vector>

namespace seec {

namespace trace {


//------------------------------------------------------------------------------
// ThreadEventBlockSequence
//------------------------------------------------------------------------------

ThreadEventBlockSequence::
  ThreadEventBlockSequence(std::vector<InputBlock> const &Blocks)
: m_Sequence(new ThreadEventBlock[Blocks.size() + 2]),
  m_BlockCount(Blocks.size())
{
  size_t Index = 1;
  for (auto &Block : Blocks) {
    // Skip the thread event block header (thread id).    
    auto Data = Block.getData().slice(sizeof(uint32_t));
    
    EventRecordBase const * const Start =
      reinterpret_cast<EventRecordBase const *>(Data.data());
    
    EventRecordBase const * const BlockEnd =
      reinterpret_cast<EventRecordBase const *>
                      (Data.end() - sizeof(EventRecordBase));
    
    EventRecordBase const * End = Start;
    while (true) {
      auto const Size = End->getEventSize();
      auto const Next = reinterpret_cast<EventRecordBase const *>(
                          reinterpret_cast<char const *>(End) + Size);
      
      if (Next <= BlockEnd && Next->getType() != EventType::None) {
        End = Next;
      }
      else {
        break;
      }
    }
    
    m_Sequence[Index] = ThreadEventBlock(*Start, *End);
    ++Index;
  }
}

namespace {

struct BlockSearchComparator {
  bool operator()(ThreadEventBlockSequence::ThreadEventBlock const &Block,
                  EventRecordBase const *Ptr)
  {
    return Block.end() < Ptr;
  }
  
  bool operator()(EventRecordBase const *Ptr,
                  ThreadEventBlockSequence::ThreadEventBlock const &Block)
  {
    return Ptr < Block.begin();
  }
};

}

llvm::Optional<EventReference>
ThreadEventBlockSequence::getReferenceTo(EventRecordBase const &Ev) const
{
  llvm::Optional<EventReference> Ret;
  
  auto const Begin = &(m_Sequence[1]);
  auto const End   = &(m_Sequence[m_BlockCount + 1]);
  auto const EvPtr = &Ev;
  
  auto const It = std::lower_bound(Begin, End, EvPtr, BlockSearchComparator());
  
  if (It != End && It->begin() <= EvPtr && EvPtr <= It->end()) {
    Ret = EventReference(Ev, *It);
  }
  
  return Ret;
}


//------------------------------------------------------------------------------
// doesLookLikeTraceFile()
//------------------------------------------------------------------------------

type_safe::boolean doesLookLikeTraceFile(char const * const Path)
{
  errno = 0;
  FILE *fp = std::fopen(Path, "r");
  if (!fp) {
    return false;
  }
  
  auto CloseOnScope = scopeExit([=] () { std::fclose(fp); });
  
  char ReadBuffer[10];
  if (std::fgets(ReadBuffer, sizeof(ReadBuffer), fp)) {
    if (strncmp(ReadBuffer, "SEECSEEC", 8) == 0) {
      return true;
    }
  }
  
  return false;
}


//------------------------------------------------------------------------------
// InputBufferAllocator
//------------------------------------------------------------------------------

InputBufferAllocator::~InputBufferAllocator()
{
  // If this trace was expanded from an archive, delete the temporary files.
  if (m_TempFiles.size()) {
    for (auto const &File : m_TempFiles) {
      if (wxFileExists(File)) {
        wxRemoveFile(File);
      }
      else if (wxDirExists(File)) {
        wxRmdir(File);
      }
    }
  }
}

seec::Maybe<InputBufferAllocator, seec::Error>
InputBufferAllocator::
  createForArchive(std::unique_ptr<wxArchiveInputStream> Input)
{
  if (!Input || !Input->IsOk()) {
    llvm::errs() << "No input or input is not OK.\n";
    
    return seec::Error{
      seec::LazyMessageByRef::create("Trace",
        {"errors", "ProcessTraceFailRead"})};
  }

  // Create a temporary directory to hold the extracted trace files.
  // TODO: Delete this directory if the rest of the process fails.
  auto const TempPath = wxFileName::CreateTempFileName("SeeC");
  wxRemoveFile(TempPath);
  if (!wxMkdir(TempPath)) {
    return seec::Error{
      seec::LazyMessageByRef::create("Trace",
        {"errors", "CreateDirectoryFail"},
        std::make_pair("path", TempPath.c_str()))};
  }

  // Attempt to read from the file.
  std::unique_ptr<wxArchiveEntry> Entry;
  std::vector<std::string> TempFiles;
  std::string TraceFilePath;
  
  while (Entry.reset(Input->GetNextEntry()), Entry) {
    // Skip dir entries, because file entries have the complete path.
    if (Entry->IsDir())
      continue;

    auto const &Name = Entry->GetName();
    wxFileName Path{Name};

    if (Name.EndsWith(".seec") &&
        Path.GetDirCount() == 1 && Path.GetDirs()[0] == "trace")
    {
      Path.RemoveDir(0);

      auto const FullPath = TempPath
                          + wxFileName::GetPathSeparator()
                          + Path.GetFullPath();

      wxFFileOutputStream Out{FullPath};
      if (!Out.IsOk()) {
        llvm::errs() << "couldn't create output for trace file.\n";
        
        return seec::Error{seec::LazyMessageByRef::create("Trace",
                            {"errors", "ProcessTraceFailRead"})};
      }

      Out.Write(*Input);
      
      TraceFilePath = FullPath.ToStdString();
      TempFiles.emplace_back(TraceFilePath);
    }
  }
  
  // Delete the temp directory also.
  TempFiles.push_back(TempPath.ToStdString());

  return InputBufferAllocator::createForFile(TraceFilePath,
                                             std::move(TempFiles));
}

seec::Maybe<InputBufferAllocator, seec::Error>
InputBufferAllocator::createForFile(llvm::StringRef Path,
                                    std::vector<std::string> TempFiles)
{
  auto MaybeBuffer =
    llvm::MemoryBuffer::getFile(Path.str(),
                                /* FileSize */ -1,
                                /* RequiresNullTerminator */ false);
  
  if (!MaybeBuffer) {
    auto Message = UnicodeString::fromUTF8(MaybeBuffer.getError().message());
    
    return Error(
      LazyMessageByRef::create("Trace",
                               {"errors", "InputBufferAllocationFail"},
                                std::make_pair("file", Path.str().c_str()),
                                std::make_pair("error", std::move(Message))));
  }
  
  char const * const InitialString = "SEECSEEC";
  auto const &Buffer = **MaybeBuffer;
  
  if (!Buffer.getBuffer().startswith(InitialString)) {
    return Error(
      LazyMessageByRef::create("Trace",
                               {"errors", "MalformedTraceFile"}));
  }
  
  llvm::Optional<InputBlock> BlockModuleBitcode;
  llvm::Optional<InputBlock> BlockProcessTrace;
  std::vector<std::vector<InputBlock>> BlocksThreadEvents;
  
  // Find the blocks.
  auto const BlockHeaderSize = sizeof(BlockType) + sizeof(uint64_t);
  char const *BlockStart = Buffer.getBufferStart() + strlen(InitialString);
  
  while (BlockStart < Buffer.getBufferEnd() - BlockHeaderSize) {
    BlockType const Type = *reinterpret_cast<BlockType const *>(BlockStart);
    uint64_t const NextBlock = *reinterpret_cast<uint64_t const *>(BlockStart + sizeof(Type));
    char const * const BlockEnd = Buffer.getBufferStart() + NextBlock;
    
    InputBlock const Block(Type, BlockStart + BlockHeaderSize, BlockEnd);
    
    if (Type == BlockType::ModuleBitcode) {
      if (BlockModuleBitcode) {
        return Error(
          LazyMessageByRef::create("Trace",
                                   {"errors", "MalformedTraceFile"}));
      }
      
      BlockModuleBitcode = Block;
    }
    else if (Type == BlockType::ProcessTrace) {
      if (BlockProcessTrace) {
        return Error(
          LazyMessageByRef::create("Trace",
                                   {"errors", "MalformedTraceFile"}));
      }
      
      BlockProcessTrace = Block;
    }
    else if (Type == BlockType::ProcessData) {
      // Nothing to do.
    }
    else if (Type == BlockType::ThreadEvents) {
      uint32_t const ID = *reinterpret_cast<uint32_t const *>(Block.getData().data());
      
      if (BlocksThreadEvents.size() < ID) {
        BlocksThreadEvents.resize(ID);
      }
      
      BlocksThreadEvents[ID - 1].push_back(Block);
    }
    else {
      return Error(
        LazyMessageByRef::create("Trace",
                                 {"errors", "MalformedTraceFile"}));
    }
    
    BlockStart = BlockEnd;
  }
  
  if (!BlockModuleBitcode || !BlockProcessTrace) {
    return Error(
      LazyMessageByRef::create("Trace",
                               {"errors", "MalformedTraceFile"}));
  }
  
  // Thread sorting.
  std::vector<ThreadEventBlockSequence> ThreadEventSequences;
  ThreadEventSequences.reserve(BlocksThreadEvents.size());
  
  for (auto &Blocks : BlocksThreadEvents) {
    ThreadEventSequences.emplace_back(Blocks);
  }
  
  return InputBufferAllocator(std::move(*MaybeBuffer),
                              *BlockModuleBitcode,
                              *BlockProcessTrace,
                              std::move(ThreadEventSequences),
                              std::move(TempFiles));
}

seec::Maybe<InputBufferAllocator, seec::Error>
InputBufferAllocator::createFor(llvm::StringRef Path)
{
  if (Path.endswith(".seec") && doesLookLikeTraceFile(Path.str().c_str())) {
    return createForFile(Path,
                         /* TempFiles */ std::vector<std::string>{});
  }
  
  auto Factory = wxArchiveClassFactory::Find(Path.str(), wxSTREAM_FILEEXT);
  if (!Factory) {
    Factory = wxArchiveClassFactory::Find(".zip", wxSTREAM_FILEEXT);
  }
  
  if (Factory) {
    return createForArchive(
      std::unique_ptr<wxArchiveInputStream>(
        Factory->NewStream(new wxFFileInputStream(Path.str()))));
  }

  return Error(
    LazyMessageByRef::create("Trace",
      {"errors", "UnknownFileType"},
      std::make_pair("file", Path.str().c_str())));
}

seec::Maybe<std::unique_ptr<llvm::Module>, seec::Error>
InputBufferAllocator::getModule(llvm::LLVMContext &Context) const
{
  // Parse the Module from the bitcode.
  auto BitcodeArray = m_BlockForModule.getData();
  
  auto MaybeMod =
    llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(
        llvm::StringRef(BitcodeArray.data(), BitcodeArray.size()),
        "bitcode"),
      Context);
  
  if (!MaybeMod) {
    std::string ErrMsg;
    
    handleAllErrors(MaybeMod.takeError(),
      [&](llvm::ErrorInfoBase &EIB){
        ErrMsg = EIB.message();
      });
    
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "ParseBitcodeFileFail"},
                                          std::make_pair("error",
                                                         ErrMsg.c_str())));
  }
  
  return std::move(*MaybeMod);
}


//------------------------------------------------------------------------------
// EventReference
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// deserializeRuntimeError
//------------------------------------------------------------------------------

std::unique_ptr<seec::runtime_errors::Arg>
deserializeRuntimeErrorArg(uint8_t Type, uint64_t Data) {
  return seec::runtime_errors::Arg::deserialize(Type, Data);
}

std::pair<std::unique_ptr<seec::runtime_errors::RunError>, EventReference>
deserializeRuntimeErrorImpl(EventRange Records) {
  using namespace seec::runtime_errors;
  
  auto EvRef = Records.begin();
  if (EvRef->getType() != EventType::RuntimeError)
    return std::make_pair(nullptr, EvRef);
  
  auto &ErrorRecord = EvRef.get<EventType::RuntimeError>();
  auto ErrorType = static_cast<RunErrorType>(ErrorRecord.getErrorType());
  
  // Read arguments.
  std::vector<std::unique_ptr<Arg>> Args;
  
  for (auto Count = ErrorRecord.getArgumentCount(); Count != 0; --Count) {
    ++EvRef;
    
    auto &ArgRecord = EvRef.get<EventType::RuntimeErrorArgument>();

    Args.emplace_back(deserializeRuntimeErrorArg(ArgRecord.getArgumentType(),
                                                 ArgRecord.getArgumentData()));
  }
  
  // Read additional errors.
  std::vector<std::unique_ptr<RunError>> Additional;
  
  for (auto Count = ErrorRecord.getAdditionalCount(); Count != 0; --Count) {
    ++EvRef;
    
    auto Read =
      deserializeRuntimeErrorImpl(rangeAfterIncluding(Records, EvRef));
    
    // If we couldn't read one of the additionals, the deserialization failed.
    if (!Read.first)
      return std::make_pair(nullptr, EvRef);
    
    // Otherwise add the additional and continue from the first record that
    // follows the additional.
    Additional.emplace_back(std::move(Read.first));
    EvRef = Read.second;
  }
  
  return std::make_pair(llvm::make_unique<RunError>(ErrorType,
                                                    std::move(Args),
                                                    std::move(Additional)),
                        EvRef);
}

std::unique_ptr<seec::runtime_errors::RunError>
deserializeRuntimeError(EventRange Records) {
  return deserializeRuntimeErrorImpl(Records).first;
}


//------------------------------------------------------------------------------
// FunctionTrace
//------------------------------------------------------------------------------

llvm::raw_ostream &operator<< (llvm::raw_ostream &Out, FunctionTrace const &T) {
  Out << "[Function Idx=" << T.getIndex()
      << ", (" << T.getThreadTimeEntered()
      << "," << T.getThreadTimeExited()
      << ")]";
  return Out;
}


//------------------------------------------------------------------------------
// ThreadTrace
//------------------------------------------------------------------------------

EventReference
ThreadTrace::getReferenceToOffset(offset_uint const Offset) const {
  auto const Ptr = m_ProcessTrace.getDataRaw(Offset);
  auto const &Ev = *reinterpret_cast<seec::trace::EventRecordBase const *>(Ptr);
  auto const MaybeEvRef = m_EventSequence.getReferenceTo(Ev);
  assert(MaybeEvRef && "malformed event sequence");
  return *MaybeEvRef;
}


//------------------------------------------------------------------------------
// ProcessTrace
//------------------------------------------------------------------------------

namespace {

uint64_t getHighestProcessTimeInThread(ThreadTrace const &Thread)
{
  auto const LastProcessTime = lastSuccessfulApply(Thread.events(),
                                  [] (EventRecordBase const &Ev) {
                                    return Ev.getProcessTime();
                                  });
  
  return LastProcessTime ? *LastProcessTime : 0;
}

uint64_t
getHighestProcessTimeInThreads(
  std::vector<std::unique_ptr<ThreadTrace>> const &Threads)
{
  std::vector<uint64_t> FinalProcessTimes;
  FinalProcessTimes.reserve(Threads.size());
  
  std::transform(Threads.begin(), Threads.end(),
                 std::back_inserter(FinalProcessTimes),
                 [] (std::unique_ptr<ThreadTrace> const &Thread) {
                   return getHighestProcessTimeInThread(*Thread);
                 });
  
  auto const MaxElement = std::max_element(FinalProcessTimes.begin(),
                                           FinalProcessTimes.end());
  
  if (MaxElement != FinalProcessTimes.end()) {
    return *MaxElement;
  }
  else {
    return 0;
  }
}

}

ProcessTrace::ProcessTrace(std::unique_ptr<InputBufferAllocator> WithAllocator,
                           std::string ModuleIdentifier,
                           uint32_t NumThreads,
                           std::vector<uint64_t> GVAddresses,
                           std::vector<offset_uint> GVInitialData,
                           std::vector<uint64_t> FAddresses,
                           std::vector<uint64_t> WithStreamsInitial)
: Allocator(std::move(WithAllocator)),
  ModuleIdentifier(std::move(ModuleIdentifier)),
  NumThreads(NumThreads),
  FinalProcessTime(0),
  GlobalVariableAddresses(std::move(GVAddresses)),
  GlobalVariableInitialData(std::move(GVInitialData)),
  FunctionAddresses(std::move(FAddresses)),
  StreamsInitial(std::move(WithStreamsInitial)),
  ThreadTraces()
{
  ThreadTraces.reserve(NumThreads);
  for (size_t i = 0; i < NumThreads; ++i) {
    ThreadIDTy ID(i);
    
    auto const Blocks = Allocator->getThreadSequence(ID);
    assert(Blocks);
    
    ThreadTraces.emplace_back(new ThreadTrace(*this, ID, *Blocks));
  }
  
  FinalProcessTime = getHighestProcessTimeInThreads(ThreadTraces);
}

seec::Maybe<std::unique_ptr<ProcessTrace>, seec::Error>
ProcessTrace::readFrom(std::unique_ptr<InputBufferAllocator> Allocator)
{
  auto const TraceBuffer = Allocator->getProcessTrace().getData();
  BinaryReader TraceReader(TraceBuffer.begin(), TraceBuffer.end());

  uint64_t Version = 0;
  TraceReader >> Version;

  if (Version != formatVersion()) {
    auto const Expected = formatVersion();
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "ProcessTraceVersionIncorrect"},
                                          std::make_pair("version_found",
                                                         int64_t(Version)),
                                          std::make_pair("version_expected",
                                                         int64_t(Expected))));
  }

  std::string ModuleIdentifier;
  std::vector<uint64_t> GlobalVariableAddresses;
  std::vector<offset_uint> GlobalVariableInitialData;
  std::vector<uint64_t> FunctionAddresses;
  std::vector<uint64_t> StreamsInitial;
  std::vector<std::unique_ptr<ThreadTrace>> ThreadTraces;

  TraceReader >> ModuleIdentifier
              >> GlobalVariableAddresses
              >> GlobalVariableInitialData
              >> FunctionAddresses
              >> StreamsInitial;

  if (TraceReader.error()) {
    using namespace seec;
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "ProcessTraceFailRead"}));
  }
  
  auto const NumThreads = Allocator->getNumberOfThreadSequences();

  return std::unique_ptr<ProcessTrace>(
            new ProcessTrace(std::move(Allocator),
                             std::move(ModuleIdentifier),
                             NumThreads,
                             std::move(GlobalVariableAddresses),
                             std::move(GlobalVariableInitialData),
                             std::move(FunctionAddresses),
                             std::move(StreamsInitial)));
}

bool ProcessTrace::writeToArchive(wxArchiveOutputStream &Stream)
{
  if (!Stream.PutNextDirEntry("trace"))
    return false;

  if (!Stream.PutNextEntry(wxString{"trace/trace.seec"}))
    return false;
  
  auto const &Buffer = Allocator->getRawTraceBuffer();
  return Stream.WriteAll(Buffer.getBufferStart(), Buffer.getBufferSize());
}

size_t ProcessTrace::getCombinedFileSize() const
{
  return Allocator->getRawTraceBuffer().getBufferSize();
}

Maybe<uint32_t>
ProcessTrace::getIndexOfFunctionAt(uint64_t const Address) const
{
  for (uint32_t i = 0; i < FunctionAddresses.size(); ++i)
    if (FunctionAddresses[i] == Address)
      return i;
  
  return Maybe<uint32_t>();
}

ThreadTrace const &ProcessTrace::getThreadTrace(uint32_t ThreadID) const {
  assert(ThreadID > 0 && ThreadID <= NumThreads);
  return *(ThreadTraces[ThreadID - 1]);
}

EventReference ProcessTrace::getEventReference(EventLocation Ev) const {
  auto const &Trace = getThreadTrace(Ev.getThreadID());
  return Trace.getReferenceToOffset(Ev.getOffset());
}


} // namespace trace (in seec)

} // namespace seec
