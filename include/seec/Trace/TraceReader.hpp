//===- include/seec/Trace/TraceReader.hpp --------------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACEREADER_HPP
#define SEEC_TRACE_TRACEREADER_HPP

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/IndexTypes.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/IR/Module.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <thread>


namespace llvm {
  class raw_ostream;
  class Function;
  class GlobalVariable;
} // namespace llvm


class wxArchiveOutputStream;
class wxArchiveInputStream;


namespace seec {

namespace runtime_errors {
  class RunError; // forward-declare for deserializeRuntimeError
} // namespace runtime_errors (in seec)

namespace trace {

class EventReference;
class ProcessTrace;
class ThreadTrace;


/// Enumerates thread-level data segment types.
///
enum class ThreadSegment {
  Trace = 1,
  Events
};


/// \brief Represents a single block of the trace.
///
class InputBlock {
public:
  BlockType getType() const { return m_Type; }
  
  llvm::ArrayRef<char> getData() const {
    return llvm::ArrayRef<char>(m_Start, m_End);
  }
  
  InputBlock(BlockType Type, char const *Start, char const *End)
  : m_Type(Type),
    m_Start(Start),
    m_End(End)
  {}
  
private:
  BlockType m_Type;
  char const *m_Start;
  char const *m_End;
};


/// \brief
///
class ThreadEventBlockSequence {
public:
  class ThreadEventBlock {
  public:
    type_safe::boolean isValid() const {
      return m_Begin != nullptr && m_End != nullptr;
    }
    
    // prev will immediately precede this in memory, if
    // this is valid.
    ThreadEventBlock const *getPrevious() const {
      ThreadEventBlock const *Ret = nullptr;
      
      if (isValid()) {
        auto const Prev = std::prev(this);
        
        if (Prev->isValid()) {
          Ret = Prev;
        }
      }
      
      return Ret;
    }
    
    // next will immediately follow this in memory, if
    // this is valid.
    ThreadEventBlock const *getNext() const {
      ThreadEventBlock const *Ret = nullptr;
      
      if (isValid()) {
        auto const Next = std::next(this);
        
        if (Next->isValid()) {
          Ret = Next;
        }
      }
      
      return Ret;
    }
    
    EventRecordBase const *begin() const { return m_Begin; }
    
    EventRecordBase const *end() const { return m_End; }
    
    ThreadEventBlock()
    : m_Begin(nullptr),
      m_End(nullptr)
    {}
    
    ThreadEventBlock(EventRecordBase const &Begin, EventRecordBase const &End)
    : m_Begin(&Begin),
      m_End(&End)
    {}
    
  private:
    EventRecordBase const *m_Begin;
    EventRecordBase const *m_End;
  };
  
  ThreadEventBlockSequence(std::vector<InputBlock> const &Blocks);
  
  ThreadEventBlock const *begin() const {
    // Skip the sentinel at the beginning.
    return &(m_Sequence[1]);
  }
  
  ThreadEventBlock const *end() const {
    // Skip the sentinel at the beginning.
    return &(m_Sequence[m_BlockCount]);
  }
  
  llvm::Optional<EventReference>
  getReferenceTo(EventRecordBase const &Ev) const;
  
private:
  // [sentinel, real blocks ... , sentinel]
  std::unique_ptr<ThreadEventBlock[]> m_Sequence;
  
  // Number of real (non-sentinel) blocks.
  size_t m_BlockCount;
};


/// \brief Check if the file at the given path looks like a regular,
///        uncompressed SeeC trace file.
///
type_safe::boolean doesLookLikeTraceFile(char const *Path);


/// \brief Gets MemoryBuffers for the various sections of a trace.
///
class InputBufferAllocator {
  /// Path to the directory containing the individual execution trace files.
  std::unique_ptr<llvm::MemoryBuffer> m_TraceBuffer;

  /// Paths for temporary files (if used).
  std::vector<std::string> m_TempFiles;
  
  InputBlock m_BlockForModule;
  
  InputBlock m_BlockForProcessTrace;
  
  std::vector<ThreadEventBlockSequence> m_BlockSequencesForThreads;

  /// \brief Constructor (no temporaries).
  ///
  InputBufferAllocator(std::unique_ptr<llvm::MemoryBuffer> TraceBuffer,
                       InputBlock BlockForModule,
                       InputBlock BlockForProcessTrace,
                       std::vector<ThreadEventBlockSequence> BlockSequences,
                       std::vector<std::string> TempFiles)
  : m_TraceBuffer(std::move(TraceBuffer)),
    m_TempFiles(std::move(TempFiles)),
    m_BlockForModule(BlockForModule),
    m_BlockForProcessTrace(BlockForProcessTrace),
    m_BlockSequencesForThreads(std::move(BlockSequences))
  {
    assert(m_BlockForModule.getType() == BlockType::ModuleBitcode);
    assert(m_BlockForProcessTrace.getType() == BlockType::ProcessTrace);
  }

public:
  /// \brief Destructor. Deletes temporary files and directories.
  ///
  ~InputBufferAllocator();

  /// \name Constructors.
  /// @{

  // No copying.
  InputBufferAllocator(InputBufferAllocator const &) = delete;
  InputBufferAllocator &operator=(InputBufferAllocator const &) = delete;

  // Moving is OK.
  InputBufferAllocator(InputBufferAllocator &&) = default;
  InputBufferAllocator &operator=(InputBufferAllocator &&) = default;

private:
  /// \brief Create an \c InputBufferAllocator for a trace archive.
  /// The trace in the archive will be extracted to a temporary file,
  /// and be deleted by the destructor of the \c InputBufferAllocator.
  /// \param Path the path to the trace archive.
  /// \return The \c InputBufferAllocator or a \c seec::Error describing the
  ///         reason why it could not be created.
  ///
  static seec::Maybe<InputBufferAllocator, seec::Error>
  createForArchive(std::unique_ptr<wxArchiveInputStream> Input);

  /// \brief Create an \c InputBufferAllocator for a trace file.
  /// \param Path the path to the trace file.
  /// \return The \c InputBufferAllocator or a \c seec::Error describing the
  ///         reason why it could not be created.
  ///
  static seec::Maybe<InputBufferAllocator, seec::Error>
  createForFile(llvm::StringRef Path, std::vector<std::string> TempFiles);

public:
  /// \brief Attempt to create an \c InputBufferAllocator.
  /// \param Path the path to the trace archive or trace file.
  /// \return The \c InputBufferAllocator or a \c seec::Error describing the
  ///         reason why it could not be created.
  ///
  static
  seec::Maybe<InputBufferAllocator, seec::Error>
  createFor(llvm::StringRef Path);

  /// @} (Constructors.)


  /// \brief Get the original, uninstrumented Module.
  ///
  seec::Maybe<std::unique_ptr<llvm::Module>, seec::Error>
  getModule(llvm::LLVMContext &Context) const;

  /// \brief Get the block holding process trace information.
  ///
  InputBlock getProcessTrace() const {
    return m_BlockForProcessTrace;
  }
  
  ///
  ///
  size_t getNumberOfThreadSequences() const {
    return m_BlockSequencesForThreads.size();
  }
  
  /// \brief
  ///
  ThreadEventBlockSequence const *getThreadSequence(ThreadIDTy ID) const {
    auto const Index = uint32_t(ID);
    
    if (Index < m_BlockSequencesForThreads.size()) {
      return &(m_BlockSequencesForThreads[Index]);
    }
    
    return nullptr;
  }
  
  llvm::MemoryBuffer const &getRawTraceBuffer() const {
    return *m_TraceBuffer;
  }
  
  llvm::ArrayRef<char> getData(offset_uint Offset, size_t Size) {
    return llvm::ArrayRef<char>(getDataRaw(Offset), Size);
  }
  
  char const *getDataRaw(offset_uint Offset) {
    assert(Offset < m_TraceBuffer->getBufferSize());
    return m_TraceBuffer->getBufferStart() + Offset;
  }
};


/// \brief A reference to a single event record in a thread trace.
///
class EventReference {
  enum class EState : unsigned {
    Valid   = 0,
    PastEnd = 1
  };
  
  /// Pointer to the event record.
  EventRecordBase const *Record;
  
  /// The event record's containing block, and state of this reference.
  llvm::PointerIntPair<ThreadEventBlockSequence::ThreadEventBlock const *,
                       /* bits for state */ 1, EState> m_BlockAndState;

public:
  /// Construct an EventReference to the event at the position given by Data.
  /// \param Data a pointer to an event record.
  EventReference(char const *Data,
                 ThreadEventBlockSequence::ThreadEventBlock const &Block)
  : Record(reinterpret_cast<EventRecordBase const *>(Data)),
    m_BlockAndState(&Block, EState::Valid)
  {}

  /// Construct an EventReference to the given event record.
  /// \param Record a reference to an event record.
  EventReference(EventRecordBase const &Record,
                 ThreadEventBlockSequence::ThreadEventBlock const &Block)
  : Record(&Record),
    m_BlockAndState(&Block, EState::Valid)
  {}

  /// Copy constructor.
  EventReference(EventReference const &Other) = default;

  /// Copy assignment.
  EventReference &operator=(EventReference const &RHS) = default;


  /// \name Access
  /// @{

  /// Get a reference to the current event, using the EventRecord appropriate
  /// for the given EventType. The current event's type must match the given
  /// EventType.
  /// \tparam ET the type of event to get an event record for.
  /// \return a const reference to the current event, of type EventRecord<ET>.
  template<EventType ET>
  EventRecord<ET> const &get() const {
    assert(Record->getType() == ET);
    return *(static_cast<EventRecord<ET> const *>(Record));
  }

  EventRecordBase const &operator*() const { return *Record; }

  EventRecordBase const *operator->() const { return Record; }


  /// \name Comparison operators
  /// @{

  bool operator==(EventReference const &RHS) const {
    return Record == RHS.Record &&
           m_BlockAndState.getInt() == RHS.m_BlockAndState.getInt();
  }

  bool operator!=(EventReference const &RHS) const {
    return !(*this == RHS);
  }

  bool operator<(EventReference const &RHS) const {
    return Record < RHS.Record ||
      (Record == RHS.Record &&
       m_BlockAndState.getInt() == EState::Valid &&
       RHS.m_BlockAndState.getInt() == EState::PastEnd);
  }

  bool operator<=(EventReference const &RHS) const {
    return *this < RHS || *this == RHS;
  }

  bool operator>(EventReference const &RHS) const {
    return !(*this <= RHS);
  }

  bool operator>=(EventReference const &RHS) const {
    return !(*this < RHS);
  }

  /// @} (Comparison operators)


  /// \name Movement operators
  /// @{

  EventReference &operator++() {
    assert(m_BlockAndState.getInt() != EState::PastEnd);
    
    auto const NextRaw = reinterpret_cast<char const *>(Record)
                          + Record->getEventSize();

    auto const NextRecord = reinterpret_cast<EventRecordBase const *>(NextRaw);
    
    if (NextRecord <= m_BlockAndState.getPointer()->end()) {
      Record = NextRecord;
    }
    else {
      auto const NextBlock = m_BlockAndState.getPointer()->getNext();
      if (NextBlock) {
        m_BlockAndState.setPointer(NextBlock);
        Record = NextBlock->begin();
      }
      else {
        m_BlockAndState.setInt(EState::PastEnd);
      }
    }
    
    return *this;
  }

  EventReference operator++(int Dummy) {
    auto Result = *this;
    ++(*this);
    return Result;
  }

  EventReference &operator--() {
    if (m_BlockAndState.getInt() == EState::Valid) {
      auto const PreviousSize = Record->getPreviousEventSize();
      
      auto const PrevRaw = reinterpret_cast<char const *>(Record) - PreviousSize;
      
      auto const PrevRecord = reinterpret_cast<EventRecordBase const *>(PrevRaw);
      
      if (PrevRecord >= m_BlockAndState.getPointer()->begin()) {
        Record = PrevRecord;
      }
      else {
        auto const PrevBlock = m_BlockAndState.getPointer()->getPrevious();
        assert(PrevBlock);
        
        m_BlockAndState.setPointer(PrevBlock);
        Record = PrevBlock->end();
      }
    }
    else {
      m_BlockAndState.setInt(EState::Valid);
    }
    
    return *this;
  }

  EventReference operator--(int Dummy) {
    auto Result = *this;
    --(*this);
    return Result;
  }

  /// @} (Movement operators)
};


/// \brief References a range of event records in a thread trace.
///
class EventRange {
  /// Reference to the first event record in the range.
  EventReference Begin;

  /// Reference to the first event record following (not in) the range.
  EventReference End;

public:
  /// \brief Create a new \c EventRange from a pair of \c EventReference.
  /// \param Begin the first event in the range.
  /// \param End the first event following (not in) the range.
  ///
  EventRange(EventReference Begin, EventReference End)
  : Begin(Begin),
    End(End)
  {}

  /// \brief Copy constructor.
  ///
  EventRange(EventRange const &) = default;

  /// \brief Copy assignment.
  ///
  EventRange &operator=(EventRange const &) = default;

  /// \brief Get a reference to the first event in the range.
  ///
  EventReference begin() const { return Begin; }

  /// \brief Get a reference to the first event following (not in) the range.
  ///
  EventReference end() const { return End; }

  /// \brief Check if the range is empty (i.e. begin() == end()).
  ///
  bool empty() const { return Begin == End; }

  /// \brief Check if the given event is contained within this range.
  ///
  bool contains(EventReference Ev) const {
    return Begin <= Ev && Ev < End;
  }
};


inline
llvm::Optional<EventRange>
getRange(ThreadEventBlockSequence const &Sequence) {
  llvm::Optional<EventRange> Ret;
  
  auto const FirstBlock = Sequence.begin();
  auto const FinalBlock = Sequence.end();
  
  if (FirstBlock != nullptr && FinalBlock != nullptr
      && FirstBlock->isValid() && FinalBlock->isValid())
  {
    auto const EvBegin = FirstBlock->begin();
    auto const EvEnd = FinalBlock->end();
    
    if (EvBegin != nullptr && EvEnd != nullptr) {
      Ret.emplace(EventReference(*EvBegin, *FirstBlock),
                  ++EventReference(*EvEnd, *FinalBlock));
    }
  }
  
  return Ret;
}


/// \brief Deserialize a \c RunError from a \c RuntimeError event record.
/// \param Records a range of events that starts with the
///                \c EventRecord<EventType::RuntimeError> and should contain
///                all subservient events.
/// \return A \c std::unique_ptr holding the recreated \c RunError if
///         deserialization was successful (otherwise holding nothing).
///
std::unique_ptr<seec::runtime_errors::RunError>
deserializeRuntimeError(EventRange Records);


/// \brief Trace information for a single Function invocation.
///
class FunctionTrace {
  friend class ThreadTrace;

  /// Trace of the thread that this Function invocation occured in.
  ThreadTrace const &Thread;
  
  /// Start event for this Function invocation (contains all trace info).
  EventRecord<EventType::FunctionStart> const &StartEv;

  /// Create a new FunctionTrace using the given trace data.
  /// \param Thread the ThreadTrace that this FunctionTrace belongs to.
  ///
  FunctionTrace(ThreadTrace const &WithThread,
                EventRecord<EventType::FunctionStart> const &WithStartEv)
  : Thread(WithThread),
    StartEv(WithStartEv)
  {}

public:
  FunctionTrace(FunctionTrace const &Other) = default;
  FunctionTrace &operator=(FunctionTrace const &RHS) = default;

  /// Get the ThreadTrace that this FunctionTrace belongs to.
  ThreadTrace const &getThread() const { return Thread; }

  /// Get the Function's index.
  uint32_t getIndex() const { return StartEv.getFunctionIndex(); }

  /// Get the offset of the start event.
  offset_uint getEventStart() const {
    return StartEv.getEventOffsetStart();
  }

  /// Get the offset of the end event.
  offset_uint getEventEnd() const {
    return StartEv.getEventOffsetEnd();
  }

  /// Get the thread time at which this Function was entered.
  uint64_t getThreadTimeEntered() const {
    return StartEv.getThreadTimeEntered();
  }

  /// Get the thread time at which this Function was exited.
  uint64_t getThreadTimeExited() const {
    return StartEv.getThreadTimeExited();
  }
};

/// Write a description of a FunctionTrace to an llvm::raw_ostream.
llvm::raw_ostream &operator<< (llvm::raw_ostream &Out, FunctionTrace const &T);


/// \brief Trace information for a single thread's execution.
///
class ThreadTrace {
  friend class ProcessTrace;
  
  ProcessTrace const &m_ProcessTrace;

  /// A unique ID assigned to the thread at run-time.
  ThreadIDTy m_ID;

  /// Information about the thread's serialized events.
  ThreadEventBlockSequence const &m_EventSequence;
 
  /// \brief Constructor
  ///
  ThreadTrace(ProcessTrace const &Parent,
              ThreadIDTy ID,
              ThreadEventBlockSequence const &EventBlockSequence)
  : m_ProcessTrace(Parent),
    m_ID(ID),
    m_EventSequence(EventBlockSequence)
  {}

public:
  /// \name Accessors
  /// @{

  /// \brief Get the ID of the thread that this trace represents.
  ///
  uint32_t getThreadID() const {
    return uint32_t(m_ID);
  }

  /// \brief Get a range containing all of the events in this thread.
  ///
  EventRange events() const {
    auto Range = getRange(m_EventSequence);
    assert(Range);
    return *Range;
  }
  
  /// \brief Get the thread event blocks for this thread.
  ///
  ThreadEventBlockSequence const &getThreadEventBlockSequence() const {
    return m_EventSequence;
  }
  
  /// \brief Get a reference to the event at the given offset in the trace.
  /// \param Offset offset of the event record in the trace file.
  /// \return a reference to the event at the specified offset.
  ///
  EventReference getReferenceToOffset(offset_uint Offset) const;
  
  /// @} (Accessors)

  /// \brief Get a \c FunctionTrace from a given offset.
  ///
  FunctionTrace
  getFunctionTrace(EventRecord<EventType::FunctionStart> const &Ev) const {
    return FunctionTrace(*this, Ev);
  }

  /// @}
};


/// \brief Trace information for a single process invocation.
///
class ProcessTrace {
  /// The allocator used to initially read the trace.
  std::unique_ptr<InputBufferAllocator> const Allocator;

  /// Name of the recorded Module.
  std::string ModuleIdentifier;

  /// Number of threads recorded.
  uint32_t NumThreads;

  /// Process time at end of recording.
  uint64_t FinalProcessTime;

  /// Global variable runtime addresses, by index.
  std::vector<uint64_t> GlobalVariableAddresses;

  /// Offsets of global variable's initial data.
  std::vector<offset_uint> GlobalVariableInitialData;

  /// Function runtime addresses, by index.
  std::vector<uint64_t> FunctionAddresses;
  
  /// Runtime addresses of the initial standard input/output streams.
  std::vector<uint64_t> StreamsInitial;

  /// Thread-specific traces, by (ThreadID - 1).
  std::vector<std::unique_ptr<ThreadTrace>> ThreadTraces;

  /// \brief Constructor.
  ///
  ProcessTrace(std::unique_ptr<InputBufferAllocator> WithAllocator,
               std::string ModuleIdentifier,
               uint32_t NumThreads,
               std::vector<uint64_t> GVAddresses,
               std::vector<offset_uint> GVInitialData,
               std::vector<uint64_t> FAddresses,
               std::vector<uint64_t> WithStreamsInitial);

public:
  /// \brief Read a ProcessTrace using an InputBufferAllocator.
  ///
  static
  seec::Maybe<std::unique_ptr<ProcessTrace>, seec::Error>
  readFrom(std::unique_ptr<InputBufferAllocator> Allocator);

  /// \brief Write execution trace to an archive.
  /// \return true iff write successful.
  ///
  bool writeToArchive(wxArchiveOutputStream &Stream);

  /// \name Accessors
  /// @{

  /// \brief Get the identifier of the Module recorded by this trace.
  ///
  std::string const &getModuleIdentifier() const { return ModuleIdentifier; }

  /// \brief Get the number of distinct threads in this process trace.
  ///
  uint32_t getNumThreads() const { return NumThreads; }

  /// \brief Get a reference to a block of data.
  /// \param Offset the offset of the data in the process' data record.
  /// \param Size the number of bytes in the block.
  ///
  llvm::ArrayRef<char> getData(offset_uint Offset, std::size_t Size) const {
    return Allocator->getData(Offset, Size);
  }
  
  /// \brief Get a raw pointer into the process' data record.
  ///
  char const *getDataRaw(offset_uint const Offset) const {
    return Allocator->getDataRaw(Offset);
  }
  
  /// \brief Get a reference to the event record at the given offset.
  ///
  template<EventType ET>
  EventRecord<ET> const &getEventAtOffset(offset_uint Offset) const {
    auto const Raw = getDataRaw(Offset);
    assert(Raw);
    return *reinterpret_cast<EventRecord<ET> const *>(Raw);
  }

  /// \brief Get the process time at the end of this trace.
  ///
  uint64_t getFinalProcessTime() const { return FinalProcessTime; }
  
  /// \brief Get the runtime addresses of the initial standard streams.
  ///
  std::vector<uint64_t> const &getStreamsInitial() const {
    return StreamsInitial;
  }

  /// \brief Get the combined size of all files.
  ///
  size_t getCombinedFileSize() const;

  /// @} (Accessors)


  /// \name Global Variables
  /// @{

  /// \brief Get the run-time address of a global variable.
  /// \param Index the index of the GlobalVariable in the Module.
  /// \return the run-time address of the specified GlobalVariable.
  ///
  uint64_t getGlobalVariableAddress(uint32_t Index) const {
    assert(Index < GlobalVariableAddresses.size());
    return GlobalVariableAddresses[Index];
  }

  /// \brief Get a reference to the block of data holding a global variable's
  /// initial state.
  /// \param Index the index of the GlobalVariable in the Module.
  /// \param Size the size of the GlobalVariable.
  /// \return an llvm::ArrayRef that references the initial state data.
  ///
  llvm::ArrayRef<char> getGlobalVariableInitialData(uint32_t Index,
                                                    std::size_t Size) const {
    assert(Index < GlobalVariableInitialData.size());
    return getData(GlobalVariableInitialData[Index], Size);
  }

  /// @} (Global Variables)


  /// \name Functions
  /// @{
  
  /// \brief Get the run-time address of a Function.
  /// \param Index the index of the Function in the Module.
  /// \return the run-time address of the specified Function.
  ///
  uint64_t getFunctionAddress(uint32_t Index) const {
    assert(Index < FunctionAddresses.size());
    return FunctionAddresses[Index];
  }
  
  /// \brief Get the index of the Function with the given run-time address.
  /// \param Address a run-time address.
  /// \return the index of the Function with the specified run-time Address, or
  ///         an unassigned Maybe if no such Function exists.
  ///
  Maybe<uint32_t> getIndexOfFunctionAt(uint64_t const Address) const;

  /// @} (Functions)


  /// \name Threads
  /// @{

  /// \brief Get a reference to the ThreadTrace for a specific thread.
  /// \param ThreadID the unique ID of the thread to get,
  ///                 where 0 < ThreadID <= getNumThreads().
  ///
  ThreadTrace const &getThreadTrace(uint32_t ThreadID) const;

  /// @} (Threads)


  /// \name Events
  /// @{

  /// \brief Get a reference to the event at the given location.
  ///
  EventReference getEventReference(EventLocation Ev) const;

  /// @}
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEREADER_HPP
