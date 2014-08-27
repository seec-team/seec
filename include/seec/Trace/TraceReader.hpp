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
  /// \brief Create an empty \c EventRange.
  ///
  EventRange()
  : Begin(nullptr),
    End(nullptr)
  {}

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

  /// \brief Get the raw offset of an event from the start of this range.
  /// \param Ev the event to find the offset of.
  /// \return the number of bytes from \c begin() to \c Ev.
  ///
  offset_uint offsetOf(EventReference Ev) const {
    assert(Begin <= Ev && Ev <= End && "Ev not in EventRange");

    auto const BeginPtr = reinterpret_cast<char const *>(&*Begin);
    auto const EvPtr = reinterpret_cast<char const *>(&*Ev);

    return static_cast<offset_uint>(EvPtr - BeginPtr);
  }

  /// \brief Get a reference to the event at the given offset in this range.
  /// \param Offset the number of bytes from \c begin() to the event.
  /// \return a reference to the event that is \c Offset bytes after \c begin().
  ///
  EventReference referenceToOffset(offset_uint Offset) const {
    auto const BeginPtr = reinterpret_cast<char const *>(&*Begin);
    return EventReference(BeginPtr + Offset);
  }

  /// \brief Get a typed reference to the event at the given offset in this
  ///        range.
  /// This differs from \c referenceToOffset in that it returns a reference
  /// to an \c EventRecord<ET> rather than an \c EventReference.
  /// \tparam ET the \c EventType of the \c EventRecord that will be
  ///         retrieved. This *must* match the event that exists at the
  ///         given \c Offset.
  /// \param Offset the number of bytes from \c begin() to the event.
  /// \return a reference to the \c EventRecord<ET> that is Offset bytes after
  ///         \c begin().
  ///
  template<EventType ET>
  EventRecord<ET> const &eventAtOffset(offset_uint Offset) const {
    auto const BeginPtr = reinterpret_cast<char const *>(&*Begin);
    return *reinterpret_cast<EventRecord<ET> const *>(BeginPtr + Offset);
  }
};


/// \brief Deserialize a \c RunError from a \c RuntimeError event record.
/// \param Records a range of events that starts with the
///                \c EventRecord<EventType::RuntimeError> and should contain
///                all subservient events.
/// \return A \c std::unique_ptr holding the recreated \c RunError if
///         deserialization was successful (otherwise holding nothing),
///         and a reference to the first event that is not associated with
///         this \c RunError.
///
std::pair<std::unique_ptr<seec::runtime_errors::RunError>, EventReference>
deserializeRuntimeError(EventRange Records);


/// \brief Trace information for a single Function invocation.
///
class FunctionTrace {
  friend class ThreadTrace;

  /// Trace of the thread that this Function invocation occured in.
  ThreadTrace const *Thread;

  char const *Data;

  /// Get the offset of Index in a serialized FunctionTrace.
  static constexpr std::size_t IndexOffset() {
    return 0;
  }

  /// Get the offset of EventStart in a serialized FunctionTrace.
  static constexpr std::size_t EventStartOffset() {
    return IndexOffset() + sizeof(uint32_t);
  }

  /// Get the offset of EventEnd in a serialized FunctionTrace.
  static constexpr std::size_t EventEndOffset() {
    return EventStartOffset() + sizeof(offset_uint);
  }

  /// Get the offset of ThreadTimeEntered in a serialized FunctionTrace.
  static constexpr std::size_t ThreadTimeEnteredOffset() {
    return EventEndOffset() + sizeof(offset_uint);
  }

  /// Get the offset of ThreadTimeExited in a serialized FunctionTrace.
  static constexpr std::size_t ThreadTimeExitedOffset() {
    return ThreadTimeEnteredOffset() + sizeof(uint64_t);
  }

  /// Get the offset of ChildList in a serialized FunctionTrace.
  static constexpr std::size_t ChildListOffset() {
    return ThreadTimeExitedOffset() + sizeof(uint64_t);
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

  /// \brief Get a list of offsets from this thread's trace information.
  ///
  llvm::ArrayRef<offset_uint> getOffsetList(offset_uint const AtOffset) const {
    auto List = Trace->getBufferStart() + AtOffset;
    auto Length = *reinterpret_cast<uint64_t const *>(List);
    auto Data = reinterpret_cast<offset_uint const *>(List + sizeof(uint64_t));
    return llvm::ArrayRef<offset_uint>(Data, static_cast<size_t>(Length));
  }
 
  /// \brief Constructor
  ///
  ThreadTrace(InputBufferAllocator &Allocator, uint32_t ID)
  : ThreadID(ID),
    Trace(std::move(Allocator.getThreadData(ID,
                                            ThreadSegment::Trace).get<0>())),
    Events(std::move(Allocator.getThreadData(ID,
                                             ThreadSegment::Events).get<0>())),
    TopLevelFunctions(getOffsetList(*reinterpret_cast<offset_uint const *>
                                    (Trace->getBufferStart())))
  {}

public:
  /// \name Accessors
  /// @{

  /// \brief Get the ID of the thread that this trace represents.
  ///
  uint32_t getThreadID() const { return ThreadID; }

  /// \brief Get a range containing all of the events in this thread.
  ///
  EventRange events() const {
    auto LastEvent = Events->getBufferEnd()
                     - sizeof(EventRecord<EventType::TraceEnd>);

    return EventRange(EventReference(Events->getBufferStart()),
                      EventReference(LastEvent));
  }

  /// @} (Accessors)


  /// \name Function traces
  /// @{

  /// \brief Get a list of offsets of top-level \c FunctionTrace records.
  ///
  llvm::ArrayRef<offset_uint> topLevelFunctions() const {
    return TopLevelFunctions;
  }

  /// \brief Get a \c FunctionTrace from a given offset.
  ///
  FunctionTrace getFunctionTrace(offset_uint const AtOffset) const {
    return FunctionTrace(*this, Trace->getBufferStart() + AtOffset);
  }

  /// @}
};


/// \brief Trace information for a single process invocation.
///
class ProcessTrace {
  /// The allocator used to initially read the trace.
  std::unique_ptr<InputBufferAllocator> const Allocator;
  
  /// Process-wide trace information.
  std::unique_ptr<llvm::MemoryBuffer> const Trace;

  /// Process-wide data.
  std::unique_ptr<llvm::MemoryBuffer> const Data;

  /// Name of the recorded Module.
  std::string ModuleIdentifier;

  /// Number of threads recorded.
  uint32_t NumThreads;

  /// Process time at end of recording.
  uint64_t FinalProcessTime;

  /// Global variable runtime addresses, by index.
  std::vector<uintptr_t> GlobalVariableAddresses;

  /// Offsets of global variable's initial data.
  std::vector<offset_uint> GlobalVariableInitialData;

  /// Function runtime addresses, by index.
  std::vector<uintptr_t> FunctionAddresses;
  
  /// Runtime addresses of the initial standard input/output streams.
  std::vector<uintptr_t> StreamsInitial;

  /// Thread-specific traces, by (ThreadID - 1).
  std::vector<std::unique_ptr<ThreadTrace>> ThreadTraces;

  /// \brief Constructor.
  ///
  ProcessTrace(std::unique_ptr<InputBufferAllocator> WithAllocator,
               std::unique_ptr<llvm::MemoryBuffer> Trace,
               std::unique_ptr<llvm::MemoryBuffer> Data,
               std::string ModuleIdentifier,
               uint32_t NumThreads,
               uint64_t FinalProcessTime,
               std::vector<uintptr_t> GVAddresses,
               std::vector<offset_uint> GVInitialData,
               std::vector<uintptr_t> FAddresses,
               std::vector<uintptr_t> WithStreamsInitial,
               std::vector<std::unique_ptr<ThreadTrace>> TTraces);

public:
  /// \brief Read a ProcessTrace using an InputBufferAllocator.
  ///
  static
  seec::Maybe<std::unique_ptr<ProcessTrace>, seec::Error>
  readFrom(std::unique_ptr<InputBufferAllocator> Allocator);

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
    auto DataStart = Data->getBufferStart() + Offset;
    auto DataPtr = reinterpret_cast<char const *>(DataStart);
    return llvm::ArrayRef<char>(DataPtr, Size);
  }
  
  /// \brief Get a raw pointer into the process' data record.
  ///
  char const *getDataRaw(offset_uint const Offset) const {
    return Data->getBufferStart() + Offset;
  }

  /// \brief Get the process time at the end of this trace.
  ///
  uint64_t getFinalProcessTime() const { return FinalProcessTime; }
  
  /// \brief Get the runtime addresses of the initial standard streams.
  ///
  std::vector<uintptr_t> const &getStreamsInitial() const {
    return StreamsInitial;
  }
  
  /// \brief Get all files used by this trace.
  ///
  seec::Maybe<std::vector<TraceFile>, seec::Error>
  getAllFileData() const;

  /// \brief Get the combined size of all files.
  ///
  Maybe<uint64_t, Error> getCombinedFileSize() const;

  /// @} (Accessors)


  /// \name Global Variables
  /// @{

  /// \brief Get the run-time address of a global variable.
  /// \param Index the index of the GlobalVariable in the Module.
  /// \return the run-time address of the specified GlobalVariable.
  ///
  uintptr_t getGlobalVariableAddress(uint32_t Index) const {
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
  uintptr_t getFunctionAddress(uint32_t Index) const {
    assert(Index < FunctionAddresses.size());
    return FunctionAddresses[Index];
  }
  
  /// \brief Get the index of the Function with the given run-time address.
  /// \param Address a run-time address.
  /// \return the index of the Function with the specified run-time Address, or
  ///         an unassigned Maybe if no such Function exists.
  ///
  Maybe<uint32_t> getIndexOfFunctionAt(uintptr_t const Address) const;

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
