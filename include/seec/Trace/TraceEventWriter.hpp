//===- include/seec/Trace/TraceEventWriter.hpp ---------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACEEVENTWRITER_HPP
#define SEEC_TRACE_TRACEEVENTWRITER_HPP

#include "seec/Trace/TraceFormat.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>


namespace seec {

namespace trace {


/// \brief Handled writing trace events to an output stream.
///
class EventWriter {
  /// The output stream to write to.
  std::unique_ptr<llvm::raw_ostream> Out;

  /// Number of bytes already written to the stream.
  offset_uint Offset;
  
  /// Size of the last-written event (in bytes).
  uint8_t PreviousEventSize;
  
  /// Offset of the last-written event for each EventType.
  offset_uint PreviousOffsets[static_cast<std::size_t>(EventType::Highest)];
  
  // Don't allow copying.
  EventWriter(EventWriter const &) = delete;
  EventWriter &operator=(EventWriter const &) = delete;
  
  /// \brief Write a block of data, and get the offset that it starts at.
  /// \param Bytes the array of bytes to be written.
  /// \return the offset that this block was written at.
  ///
  offset_uint write(llvm::ArrayRef<char> Bytes) {
    // If the stream doesn't exist, silently ignore the write request.
    if (!Out)
      return 0;
    
    auto const Size = Bytes.size();
    Out->write(Bytes.data(), Size);

    // Update the current offset and return the original value.
    auto const WrittenAt = Offset;
    Offset += Size;
    return WrittenAt;
  }
  
public:
  /// \brief Constructor.
  ///
  EventWriter(std::unique_ptr<llvm::raw_ostream> OutStream)
  : Out(std::move(OutStream)),
    Offset(0),
    PreviousEventSize(0),
    PreviousOffsets()
  {
    constexpr auto NumEventTypes = static_cast<std::size_t>(EventType::Highest);
    for (std::size_t i = 0; i < NumEventTypes; ++i) {
      PreviousOffsets[i] = noOffset();
    }
  }
  
  /// \name Accessors
  /// @{
  
  /// \brief Get the number of bytes already written to the stream.
  offset_uint offset() const { return Offset; }
  
  /// \brief Get the size of the last-written event.
  uint8_t previousEventSize() const { return PreviousEventSize; }

  /// \brief Get the offset of the last-written event of type Type.
  offset_uint getPreviousOffsetOf(EventType Type) const {
    assert(Type != EventType::Highest);
    return PreviousOffsets[static_cast<std::size_t>(Type)];
  }
  
  /// @} (Accessors)
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Close this EventWriter's output stream.
  void close() {
    Out.reset(nullptr);
  }
  
  /// \brief Construct a record and then write it as a block of data.
  /// \tparam ET the type of event to construct a record for.
  /// \tparam ArgTypes the types of arguments to pass to the record constructor.
  /// \param Args the arguments to pass to the record constructor.
  /// \return the value of EventsOutOffset prior to writing the block of data.
  template<EventType ET,
           typename... ArgTypes>
  offset_uint write(ArgTypes&&... Args) {
    EventRecord<ET> Record(PreviousEventSize, std::forward<ArgTypes>(Args)...);

    // Write the record as a block of bytes.
    auto const BytePtr = reinterpret_cast<char const *>(&Record);
    auto const Bytes = llvm::ArrayRef<char>(BytePtr, sizeof(Record));
    auto const Offset = write(Bytes);
    
    PreviousEventSize = sizeof(Record);
    PreviousOffsets[static_cast<std::size_t>(ET)] = Offset;
    
    return Offset;
  }
  
  /// @} (Mutators)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEEVENTWRITER_HPP
