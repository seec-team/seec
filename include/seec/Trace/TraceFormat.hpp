//===- include/seec/Trace/TraceFormat.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_TRACEFORMAT_HPP
#define SEEC_TRACE_TRACEFORMAT_HPP

#include "seec/Preprocessor/Apply.h"
#include "seec/Util/Maybe.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <utility>
#include <memory>
#include <type_traits>

namespace seec {

namespace runtime_errors {

class Arg;

}

namespace trace {

/// Type used for offsets into trace files.
typedef uint64_t offset_uint;

/// Value used to represent an invalid or nonexistant offset.
inline offset_uint noOffset() {
  return std::numeric_limits<offset_uint>::max();
}

/// Version of the trace storage format.
constexpr inline uint64_t formatVersion() { return 2; }

/// ThreadID used to indicate that an event location refers to the initial
/// state of the process.
constexpr inline uint32_t initialDataThreadID() { return 0; }

/// ProcessTime used to refer to the initial state of the process.
constexpr inline uint64_t initialDataProcessTime() { return 0; }


/// Enumeration of possible event types.
enum class EventType : uint8_t {
  None = 0,
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) NAME,
#include "seec/Trace/Events.def"
  Highest
};

/// \brief Get a string indicating describing the value of Type.
char const *describe(EventType Type);


//------------------------------------------------------------------------------
// Event traits
//------------------------------------------------------------------------------

/// For events that represent the start of an event block. That is, events with
/// this trait can be applied separately to any prior events.
template<EventType ET>
struct is_block_start { static bool const value = false; };

/// For events that contain additional information for a preceding event.
template<EventType ET>
struct is_subservient { static bool const value = false; };

/// For events that affect the currently active function.
template<EventType ET>
struct is_function_level { static bool const value = false; };

/// For events that set the currently active instruction.
template<EventType ET>
struct is_instruction { static bool const value = false; };

/// For events that affect the shared process state.
template<EventType ET>
struct modifies_shared_state { static bool const value = false; };

/// For events that set memory state (not clearing).
template<EventType ET>
struct is_memory_state { static bool const value = false; };

/// Dummy trait for events that define no other traits.
template<EventType ET>
struct no_traits { static bool const value = false; };

#define SEEC_PP_TRAIT(EVENT_NAME, TRAIT_NAME)                                  \
template<>                                                                     \
struct TRAIT_NAME<EventType::EVENT_NAME> { static bool const value = true; };  \

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
SEEC_PP_APPLY(SEEC_PP_TRAIT, TRAITS)
#include "seec/Trace/Events.def"

#undef SEEC_PP_TRAIT


//------------------------------------------------------------------------------
// FunctionRecord
//------------------------------------------------------------------------------

///
struct FunctionRecord {
  uint32_t Index;

  offset_uint EventOffsetStart;

  offset_uint EventOffsetEnd;

  uint64_t ThreadTimeEntered;

  uint64_t ThreadTimeExited;

  offset_uint ChildListOffset;

  offset_uint NonLocalChangeListOffset;
};


//------------------------------------------------------------------------------
// RuntimeValueRecord
//------------------------------------------------------------------------------

union RuntimeValueRecord {
  uint64_t UInt64;
  
  uintptr_t UIntPtr;
  
  float Float;
  
  double Double;
  
  /// \brief Default construct.
  RuntimeValueRecord()
  {}
  
  /// \brief Copy construct.
  RuntimeValueRecord(RuntimeValueRecord const &Other) = default;
  
  /// \brief Construct a new record holding a uint32_t.
  RuntimeValueRecord(uint32_t Value)
  : UInt64(Value)
  {}
  
  /// \brief Construct a new record holding a uint64_t.
  RuntimeValueRecord(uint64_t Value)
  : UInt64(Value)
  {}
  
  /// \brief Create a new record holding a uintptr_t.
  static RuntimeValueRecord ofPointer(uintptr_t Value) {
    RuntimeValueRecord Record;
    Record.UIntPtr = Value;
    return Record;
  }
  
  /// \brief Construct a new record holding a float.
  explicit RuntimeValueRecord(float Value)
  : Float(Value)
  {}
  
  /// \brief Construct a new record holding a double.
  explicit RuntimeValueRecord(double Value)
  : Double(Value)
  {}
  
  /// \brief Copy assign.
  RuntimeValueRecord &operator=(RuntimeValueRecord const &RHS) = default;
};

llvm::raw_ostream & operator<<(llvm::raw_ostream &Out,
                               RuntimeValueRecord const &Record);


//------------------------------------------------------------------------------
// EventRecordBase
//------------------------------------------------------------------------------

// forward-declaration
template<EventType ET>
class EventRecord;

/// \brief Base class for all event records.
class EventRecordBase {
  EventType Type;
  
  uint8_t PreviousEventSize;
  
public:
  EventRecordBase(EventType Type, uint8_t PreviousEventSize)
  : Type(Type),
    PreviousEventSize(PreviousEventSize)
  {}
  
  
  /// \name Conversion
  /// @{
  
  template<EventType ET>
  EventRecord<ET> const &as() const {
    assert(Type == ET);
    return *(static_cast<EventRecord<ET> const *>(this));
  }
  
  /// @}
  
  
  /// \name Query event properties
  /// @{
  
  /// Get the type of this event.
  EventType getType() const { return Type; }
  
  /// Get the size of the event immediately preceding this event.
  uint8_t getPreviousEventSize() const { return PreviousEventSize; }
  
  /// Get the size of this event.
  std::size_t getEventSize() const;
  
  /// Get the value of this event's ProcessTime, if it has one.
  seec::util::Maybe<uint64_t> getProcessTime() const;
  
  /// Get the value of this event's ThreadTime, if it has one.
  seec::util::Maybe<uint64_t> getThreadTime() const;
  
  /// Get the value of this event's Index member, if it has one.
  seec::util::Maybe<uint32_t> getIndex() const;
  
  /// Check if this event has the is_block_start trait.
  bool isBlockStart() const {
    switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
      case EventType::NAME: return is_block_start<EventType::NAME>::value;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
    
    return false;
  }
  
  /// Check if this event has the is_subservient trait.
  bool isSubservient() const {
    switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
      case EventType::NAME: return is_subservient<EventType::NAME>::value;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
    
    return false;
  }
  
  /// Check if this event has the is_function_level trait.
  bool isFunctionLevel() const {
    switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
      case EventType::NAME: return is_function_level<EventType::NAME>::value;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
    
    return false;
  }
  
  /// Check if this event has the is_instruction trait.
  bool isInstruction() const {
    switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS) \
      case EventType::NAME: return is_instruction<EventType::NAME>::value;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
    
    return false;
  }
  
  /// Check if this event has the modifies_shared_state trait.
  bool modifiesSharedState() const {
    switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
      case EventType::NAME:                                                    \
        return modifies_shared_state<EventType::NAME>::value;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
    
    return false;
  }
  
  /// Check if this event has the is_memory_state trait.
  bool isMemoryState() const {
    switch (Type) {
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
      case EventType::NAME:                                                    \
        return is_memory_state<EventType::NAME>::value;
#include "seec/Trace/Events.def"
      default: llvm_unreachable("Reference to unknown event type!");
    }
    
    return false;
  }
  
  /// @} (Query event properties)
};

llvm::raw_ostream & operator<<(llvm::raw_ostream &Out,
                               EventRecordBase const &Event);


//------------------------------------------------------------------------------
// Event records
//------------------------------------------------------------------------------

template<EventType ET>
class EventRecord : public EventRecordBase {};

#define SEEC_PP_MEMBER(TYPE, NAME) TYPE NAME;
#define SEEC_PP_ACCESSOR(TYPE, NAME)                                           \
  TYPE const &get##NAME() const { return NAME; }                               \
  static std::size_t sizeof##NAME() { return sizeof(NAME); }                   \
  typedef TYPE typeof##NAME;
#define SEEC_PP_PARAMETER(TYPE, NAME) , TYPE NAME
#define SEEC_PP_INITIALIZE(TYPE, NAME) , NAME(NAME)

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
template<>                                                                     \
class EventRecord<EventType::NAME> : public EventRecordBase {                  \
private:                                                                       \
  SEEC_PP_APPLY(SEEC_PP_MEMBER, MEMBERS)                                       \
public:                                                                        \
  SEEC_PP_APPLY(SEEC_PP_ACCESSOR, MEMBERS)                                     \
                                                                               \
  EventRecord(uint8_t PreviousEventSize                                        \
              SEEC_PP_APPLY(SEEC_PP_PARAMETER, MEMBERS)                        \
              )                                                                \
  : EventRecordBase(EventType::NAME, PreviousEventSize)                        \
    SEEC_PP_APPLY(SEEC_PP_INITIALIZE, MEMBERS)                                 \
  {}                                                                           \
};

#include "seec/Trace/Events.def"

#undef SEEC_PP_MEMBER
#undef SEEC_PP_ACCESSOR
#undef SEEC_PP_PARAMETER
#undef SEEC_PP_INITIALIZE


// Declare raw_ostream output for all event records.
#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,                          \
                              EventRecord<EventType::NAME> const &Event);

#include "seec/Trace/Events.def"


/// \brief Holds the thread and offset of an event record.
///
class EventLocation {
  /// The thread that contains the event.
  uint32_t ThreadID;
  
  /// The offset of the event in the thread's event trace.
  offset_uint Offset;
  
public:
  /// Construct a new empty EventLocation.
  EventLocation()
  : ThreadID(0),
    Offset(noOffset())
  {}
  
  /// Construct a new EventLocation.
  EventLocation(uint32_t EventThreadID, offset_uint EventOffset)
  : ThreadID(EventThreadID),
    Offset(EventOffset)
  {}
  
  EventLocation(EventLocation const &) = default;
  
  EventLocation &operator=(EventLocation const &) = default;
  
  /// Get the thread ID of the thread that contains the event.
  uint32_t getThreadID() const { return ThreadID; }
  
  /// Check whether this event location has a legitimate offset.
  bool hasOffset() const { return Offset != noOffset(); }
  
  /// Get the offset of the event in the thread's event trace.
  offset_uint getOffset() const { return Offset; }
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEFORMAT_HPP
