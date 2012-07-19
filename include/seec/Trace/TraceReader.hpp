//===- include/seec/Trace/TraceReader.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_TRACEREADER_HPP
#define SEEC_TRACE_TRACEREADER_HPP

#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceStorage.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/Dispatch.hpp"

#include "llvm/ADT/ArrayRef.h"
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

namespace seec {

namespace runtime_errors {

class RunError; // forward-declare for deserializeRuntimeError

} // namespace runtime_errors (in seec)

namespace trace {
  
class ThreadTrace; // forward-declare for FunctionTrace


/// \brief A reference to a single event record in a thread trace.
///
class EventReference {
  /// Pointer to the event record.
  EventRecordBase const *Record;
  
public:
  /// Construct an EventReference to the event at the position given by Data.
  /// \param Data a pointer to an event record.
  EventReference(char const *Data)
  : Record(reinterpret_cast<EventRecordBase const *>(Data))
  {}
  
  /// Construct an EventReference to the given event record.
  /// \param Record a reference to an event record.
  EventReference(EventRecordBase const &Record)
  : Record(&Record)
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
  
  /// Get the result of applying the appropriate predicate to the underlying
  /// event record. An appropriate predicate from the supplied predicates will
  /// be selected using the dispatch() function.
  /// \tparam PredTs the types of the predicates.
  /// \param Preds the predicates.
  /// \return a seec::util::Maybe with element type based on the return types
  ///         of the predicates. If no appropriate predicate was found for the
  ///         event record, the Maybe will be unassigned.
  template<typename... PredTs>
  typename seec::dispatch_impl::ReturnType<getDefaultDispatchFlagSet(),
                                           PredTs...>::type
  dispatch(PredTs &&...Preds) const {
    switch(Record->getType()) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
      case EventType::NAME:                                                    \
        {                                                                      \
          auto Ptr = static_cast<EventRecord<EventType::NAME> const *>(Record);\
          return seec::dispatch(*Ptr, std::forward<PredTs>(Preds)...);         \
        }
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
  }
  
  /// @} (Access)
  
  
  /// \name Comparison operators
  /// @{
  
  bool operator==(EventReference const &RHS) const {
    return Record == RHS.Record;
  }
  
  bool operator!=(EventReference const &RHS) const {
    return Record != RHS.Record;
  }
  
  bool operator<(EventReference const &RHS) const {
    return Record < RHS.Record;
  }
  
  bool operator<=(EventReference const &RHS) const {
    return Record <= RHS.Record;
  }
  
  bool operator>(EventReference const &RHS) const {
    return Record > RHS.Record;
  }
  
  bool operator>=(EventReference const &RHS) const {
    return Record >= RHS.Record;
  }
  
  /// @} (Comparison operators)
  
  
  /// \name Movement operators
  /// @{
  
  EventReference &operator++() {
    auto Data = reinterpret_cast<char const *>(Record);
    Data += Record->getEventSize();
    Record = reinterpret_cast<EventRecordBase const *>(Data);
    return *this;
  }
  
  EventReference operator++(int Dummy) {
    auto Result = *this;
    ++(*this);
    return Result;
  }
  
  EventReference operator+(std::size_t Steps) {
    auto Result = *this;
    
    for (std::size_t i = 0; i < Steps; ++i)
      ++Result;
    
    return Result;
  }
  
  EventReference &operator--() {
    auto PreviousSize = Record->getPreviousEventSize();
    auto Data = reinterpret_cast<char const *>(Record) - PreviousSize;
    Record = reinterpret_cast<EventRecordBase const *>(Data);
    return *this;
  }
  
  EventReference operator--(int Dummy) {
    auto Result = *this;
    --(*this);
    return Result;
  }
  
  EventReference operator-(std::size_t Steps) {
    auto Result = *this;
    
    for (std::size_t i = 0; i < Steps; ++i)
      --Result;
    
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
  EventRange()
  : Begin(nullptr),
    End(nullptr)
  {}
  
  EventRange(EventReference Begin, EventReference End)
  : Begin(Begin),
    End(End)
  {}
  
  EventRange(EventRange const &) = default;
  
  EventRange &operator=(EventRange const &) = default;
  
  EventReference begin() const { return Begin; }
  
  EventReference end() const { return End; }
  
  bool empty() const { return Begin == End; }
  
  bool contains(EventReference Ev) const {
    return Begin <= Ev && Ev < End;
  }
  
  offset_uint offsetOf(EventReference Ev) const {
    assert(Begin <= Ev && Ev <= End && "Ev not in EventRange");
    
    auto const BeginPtr = reinterpret_cast<char const *>(&*Begin);
    auto const EvPtr = reinterpret_cast<char const *>(&*Ev);
    
    return EvPtr - BeginPtr;
  }
  
  EventReference referenceToOffset(offset_uint Offset) const {
    auto const BeginPtr = reinterpret_cast<char const *>(&*Begin);
    return EventReference(BeginPtr + Offset);
  }
  
  template<EventType ET>
  EventRecord<ET> const &eventAtOffset(offset_uint Offset) const {
    auto const BeginPtr = reinterpret_cast<char const *>(&*Begin);
    return *reinterpret_cast<EventRecord<ET> const *>(BeginPtr + Offset);
  }
};


/// Attempt to recreate a RunError from a RuntimeError event record.
/// \param Record the RuntimeErrorRecord to recreate the RunError from.
/// \param End end of the Event array (e.g. end of thread containing Record).
/// \return A unique_ptr holding the recreated RunError if deserialization was
///         successful, otherwise an empty unique_ptr.
std::unique_ptr<seec::runtime_errors::RunError>
deserializeRuntimeError(EventRange Records);


/// \brief Trace information for a single Function invocation.
///
class FunctionTrace {
  friend class ThreadTrace;

  /// Trace of the thread that this Function invocation occured in.
  ThreadTrace const *Thread;
  
  char const *Data;

  /// Get the offset of Index in a serialized FunctionTrace.
  static constexpr size_t IndexOffset() {
    return 0;
  }

  /// Get the offset of EventStart in a serialized FunctionTrace.
  static constexpr size_t EventStartOffset() {
    return IndexOffset() + sizeof(uint32_t);
  }

  /// Get the offset of EventEnd in a serialized FunctionTrace.
  static constexpr size_t EventEndOffset() {
    return EventStartOffset() + sizeof(offset_uint);
  }

  /// Get the offset of ThreadTimeEntered in a serialized FunctionTrace.
  static constexpr size_t ThreadTimeEnteredOffset() {
    return EventEndOffset() + sizeof(offset_uint);
  }

  /// Get the offset of ThreadTimeExited in a serialized FunctionTrace.
  static constexpr size_t ThreadTimeExitedOffset() {
    return ThreadTimeEnteredOffset() + sizeof(uint64_t);
  }

  /// Get the offset of ChildList in a serialized FunctionTrace.
  static constexpr size_t ChildListOffset() {
    return ThreadTimeExitedOffset() + sizeof(uint64_t);
  }

  /// Get the offset of NonLocalChangeList in a serialized FunctionTrace.
  static constexpr size_t NonLocalChangeListOffset() {
    return ChildListOffset() + sizeof(offset_uint);
  }

  /// Create a new FunctionTrace using the given trace data.
  /// \param Thread the ThreadTrace that this FunctionTrace belongs to.
  /// \param Data pointer to the serialized FunctionTrace.
  FunctionTrace(ThreadTrace const &Thread, char const *Data)
  : Thread(&Thread),
    Data(Data)
  {}

public:
  /// Copy constructor.
  FunctionTrace(FunctionTrace const &Other) = default;
  
  /// Copy assignment.
  FunctionTrace &operator=(FunctionTrace const &RHS) = default;

  /// Get the ThreadTrace that this FunctionTrace belongs to.
  ThreadTrace const &getThread() const { return *Thread; }

  /// Get the Function's index.
  uint32_t getIndex() const {
    return *reinterpret_cast<uint32_t const *>(Data + IndexOffset());
  }

  /// Get the offset of the start event.
  offset_uint getEventStart() const {
    return *reinterpret_cast<offset_uint const *>(Data + EventStartOffset());
  }

  /// Get the offset of the end event.
  offset_uint getEventEnd() const {
    return *reinterpret_cast<offset_uint const *>(Data + EventEndOffset());
  }

  /// Get the thread time at which this Function was entered.
  uint64_t getThreadTimeEntered() const {
    auto ValuePtr = Data + ThreadTimeEnteredOffset();
    return *reinterpret_cast<uint64_t const *>(ValuePtr);
  }

  /// Get the thread time at which this Function was exited.
  uint64_t getThreadTimeExited() const {
    auto ValuePtr = Data + ThreadTimeExitedOffset();
    return *reinterpret_cast<uint64_t const *>(ValuePtr);
  }

  /// Get the list of child FunctionTraces.
  llvm::ArrayRef<offset_uint> getChildList() const;

  /// Get the list of non-local memory changes.
  llvm::ArrayRef<offset_uint> getNonLocalChangeList() const;
};

/// Write a description of a FunctionTrace to an llvm::raw_ostream.
llvm::raw_ostream &operator<< (llvm::raw_ostream &Out, FunctionTrace const &T);


/// \brief Trace information for a single thread's execution.
///
class ThreadTrace {
  friend class ProcessTrace;

  /// A unique ID assigned to the thread at run-time.
  uint32_t ThreadID;

  /// Holds the thread's serialized trace information.
  std::unique_ptr<llvm::MemoryBuffer> Trace;

  /// Holds the thread's serialized events.
  std::unique_ptr<llvm::MemoryBuffer> Events;

  /// A list of offsets of top-level FunctionTraces.
  llvm::ArrayRef<offset_uint> TopLevelFunctions;

  /// Constructor
  ThreadTrace(InputBufferAllocator &Allocator, uint32_t ID)
  : ThreadID(ID),
    Trace(Allocator.getThreadData(ID, "trace")),
    Events(Allocator.getThreadData(ID, "events")),
    TopLevelFunctions(getOffsetList(*reinterpret_cast<offset_uint const *>
                                    (Trace->getBufferStart())))
  {}

public:
  /// \name Accessors
  /// @{

  /// Get the ID of the thread that this trace represents.
  uint32_t getThreadID() const { return ThreadID; }
  
  /// Get a range containing all of the events in this thread.
  EventRange events() const {
    return EventRange(EventReference(Events->getBufferStart()),
                      EventReference(Events->getBufferEnd()));
  }
  
  /// Get a list of offsets from this thread's trace information.
  llvm::ArrayRef<offset_uint> getOffsetList(offset_uint const AtOffset) const {
    auto List = Trace->getBufferStart() + AtOffset;
    auto Length = *reinterpret_cast<uint64_t const *>(List);
    auto Data = reinterpret_cast<offset_uint const *>(List + sizeof(uint64_t));
    return llvm::ArrayRef<offset_uint>(Data, static_cast<size_t>(Length));
  }
  
  /// @} (Accessors)
  
  
  /// \name Functions
  /// @{

  /// Get a list of offsets of top-level FunctionTraces.
  llvm::ArrayRef<offset_uint> topLevelFunctions() const {
    return TopLevelFunctions;
  }

  /// Get a FunctionTrace from a given offset.
  FunctionTrace getFunctionTrace(offset_uint const AtOffset) const {
    return FunctionTrace(*this, Trace->getBufferStart() + AtOffset);
  }

  /// Get a dynamically-allocated FunctionTrace from a given offset.
  std::unique_ptr<FunctionTrace>
  makeFunctionTrace(offset_uint const AtOffset) const {
    return std::unique_ptr<FunctionTrace>(
      new FunctionTrace(*this, Trace->getBufferStart() + AtOffset));
  }
  
  /// @}
};


/// \brief Trace information for a single process invocation.
///
class ProcessTrace {
  /// Gets MemoryBuffers for input files.
  InputBufferAllocator &BufferAllocator;

  /// Process-wide trace information.
  std::unique_ptr<llvm::MemoryBuffer> const Trace;

  /// Process-wide data.
  std::unique_ptr<llvm::MemoryBuffer> const Data;

  /// Trace format version.
  uint64_t Version;

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

  /// Thread-specific traces, by (ThreadID - 1).
  // mutable because we lazily construct the ThreadTrace objects.
  mutable std::vector<std::unique_ptr<ThreadTrace>> ThreadTraces;
  
  /// Constructor.
  ProcessTrace(InputBufferAllocator &Allocator,
               std::unique_ptr<llvm::MemoryBuffer> &&Trace,
               std::unique_ptr<llvm::MemoryBuffer> &&Data,
               uint64_t Version,
               std::string &&ModuleIdentifier,
               uint32_t NumThreads,
               uint64_t FinalProcessTime,
               std::vector<uint64_t> &&GVAddresses,
               std::vector<offset_uint> &&GVInitialData,
               std::vector<uint64_t> &&FAddresses,
               std::vector<std::unique_ptr<ThreadTrace>> &&TTraces);

public:
  /// Read a ProcessTrace using an InputBufferAllocator.
  static
  seec::util::Maybe<std::unique_ptr<ProcessTrace>,
                    std::unique_ptr<seec::Error>>
  readFrom(InputBufferAllocator &Allocator);

  /// \name Accessors
  /// @{

  /// Get the identifier of the Module recorded by this trace.
  std::string const &getModuleIdentifier() const { return ModuleIdentifier; }

  /// Get the number of distinct threads in this process trace.
  uint32_t getNumThreads() const { return NumThreads; }
  
  /// Get a reference to a block of data.
  /// \param Offset the offset of the data in the process' data record.
  /// \param Size the number of bytes in the block.
  llvm::ArrayRef<char> getData(offset_uint Offset, std::size_t Size) const {
    auto DataStart = Data->getBufferStart() + Offset;
    auto DataPtr = reinterpret_cast<char const *>(DataStart);
    return llvm::ArrayRef<char>(DataPtr, Size);
  }
  
  /// Get the process time at the end of this trace.
  uint64_t getFinalProcessTime() const { return FinalProcessTime; }

  /// @} (Accessors)
  
  
  /// \name Global Variables
  /// @{
  
  /// Get the run-time address of a global variable.
  /// \param Index the index of the GlobalVariable in the Module.
  /// \return the run-time address of the specified GlobalVariable.
  uint64_t getGlobalVariableAddress(uint32_t Index) const {
    assert(Index < GlobalVariableAddresses.size());
    return GlobalVariableAddresses[Index];
  }
  
  /// Get a reference to the block of data holding a global variable's initial
  /// state.
  /// \param Index the index of the GlobalVariable in the Module.
  /// \param Size the size of the GlobalVariable.
  /// \return an llvm::ArrayRef that references the initial state data.
  llvm::ArrayRef<char> getGlobalVariableInitialData(uint32_t Index,
                                                    std::size_t Size) const {
    assert(Index < GlobalVariableInitialData.size());
    return getData(GlobalVariableInitialData[Index], Size);
  }
  
  /// @} (Global Variables)
  
  
  /// \name Functions
  /// @{
  
  /// @} (Functions)
  
  
  /// \name Threads
  /// @{
  
  /// Get a reference to the ThreadTrace for a specific thread.
  /// \param ThreadID the unique ID of the thread to get,
  ///                 where 0 < ThreadID <= getNumThreads().
  ThreadTrace const &getThreadTrace(uint32_t ThreadID) const;
  
  /// @} (Threads)
  
  
  /// \name Events
  /// @{
  
  EventReference getEventReference(EventLocation Ev) const;
  
  /// @}
};

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEREADER_HPP
