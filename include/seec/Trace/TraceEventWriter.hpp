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
#include "seec/Trace/TraceStorage.hpp"

#include "llvm/ADT/ArrayRef.h"

#include <memory>


namespace seec {

namespace trace {


/// \brief Handled writing trace events to an output stream.
///
class EventWriter {
  /// The output stream to write to.
  std::unique_ptr<OutputBlockThreadEventStream> Out;

  /// Size of the last-written event (in bytes).
  uint8_t PreviousEventSize;
  
  // Don't allow copying.
  EventWriter(EventWriter const &) = delete;
  EventWriter &operator=(EventWriter const &) = delete;
  
  /// \brief Write a block of data, and get the offset that it starts at.
  /// \param Bytes the array of bytes to be written.
  /// \return the offset that this block was written at.
  ///
  llvm::Optional<OutputBlock::WriteRecord> write(llvm::ArrayRef<char> Bytes) {
    llvm::Optional<OutputBlock::WriteRecord> Ret;
    
    // If the stream doesn't exist, silently ignore the write request.
    if (Out) {
      if (auto Result = Out->rewritableWrite(Bytes.data(), Bytes.size())) {
        Ret.emplace(*Result);
      }
    }
    
    return Ret;
  }
  
public:
  /// \brief Constructor.
  ///
  EventWriter()
  : Out(),
    PreviousEventSize(0)
  {}
  
  
  /// \name Accessors
  /// @{
  
  /// \brief Get the size of the last-written event.
  uint8_t previousEventSize() const { return PreviousEventSize; }
  
  /// @} (Accessors)
  
  
  /// \name Writing control
  /// @{
  
  /// \brief Open this EventWriter's output stream.
  ///
  void open(std::unique_ptr<OutputBlockThreadEventStream> Stream) {
    Out = std::move(Stream);
  }
  
  /// \brief Close this EventWriter's output stream.
  ///
  void close() {
    Out.reset(nullptr);
  }
  
  /// @} (Writing control)
  
  
  /// \name Event writing
  /// @{
  
  template<EventType ET>
  struct EventWriteRecord {
    uint8_t const PrecedingEventSize;
    
    offset_uint const Offset;
    
    OutputBlock::WriteRecord WriteRecord;
  
    EventWriteRecord(uint8_t const WithPrecedingEventSize,
                     OutputBlock::WriteRecord WithWriteRecord)
    : PrecedingEventSize(WithPrecedingEventSize),
      Offset(WithWriteRecord.getOffset()),
      WriteRecord(WithWriteRecord)
    {}
  };
  
  /// \brief Construct a record and then write it as a block of data.
  /// \tparam ET the type of event to construct a record for.
  /// \tparam ArgTypes the types of arguments to pass to the record constructor.
  /// \param Args the arguments to pass to the record constructor.
  /// \return the value of EventsOutOffset prior to writing the block of data.
  template<EventType ET,
           typename... ArgTypes>
  llvm::Optional<EventWriteRecord<ET>> write(ArgTypes&&... Args) {
    llvm::Optional<EventWriteRecord<ET>> Ret;
    
    if (Out) {
      // Construct the event record.
      EventRecord<ET> Record(PreviousEventSize,
                             std::forward<ArgTypes>(Args)...);
      
      // Write the record as a block of bytes.
      auto const BytePtr = reinterpret_cast<char const *>(&Record);
      auto const Bytes = llvm::ArrayRef<char>(BytePtr, sizeof(Record));
      
      auto WriteRecord = write(Bytes);
      if (WriteRecord) {
        Ret.emplace(PreviousEventSize, *WriteRecord);
        
        PreviousEventSize = sizeof(Record);
      }
    }
    
    return Ret;
  }
  
  /// \brief Write over a previously-written record.
  ///
  template<EventType ET, typename... ArgTypes>
  llvm::Optional<EventWriteRecord<ET>>
  rewrite(EventWriteRecord<ET> &PreviousWrite, ArgTypes&&... Args)
  {
    llvm::Optional<EventWriteRecord<ET>> Ret;
    
    if (Out) {
      // Construct the event record.
      EventRecord<ET> Record(PreviousWrite.PrecedingEventSize,
                             std::forward<ArgTypes>(Args)...);
      
      // Write the record as a block of bytes.
      auto const BytePtr = reinterpret_cast<char const *>(&Record);
      
      // rewrite
      auto const Result = PreviousWrite.WriteRecord.rewrite(BytePtr,
                                                            sizeof(Record));
      
      if (Result) {
        Ret.emplace(PreviousWrite);
      }
    }
    
    return Ret;
  }
  
  /// @} (Event writing)
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACEEVENTWRITER_HPP
