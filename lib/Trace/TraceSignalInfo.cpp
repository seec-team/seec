//===- lib/Trace/TraceSignalInfo.cpp --------------------------------------===//
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

#include "seec/Trace/TraceReader.hpp"
#include "seec/Trace/TraceSignalInfo.hpp"
#include "seec/Trace/TraceStorage.hpp"

namespace seec
{

namespace trace
{

constexpr size_t getOutDataLength() { return 256; }
constexpr size_t getMaxNameLength() { return 16; }

llvm::Optional<CaughtSignalInfo>
CaughtSignalInfo::readFrom(InputBlock const &FromBlock)
{
  llvm::Optional<CaughtSignalInfo> RetVal;
  
  if (FromBlock.getType() == BlockType::SignalInfo) {
    auto const Data = FromBlock.getData();
    
    if (Data.size() >= getOutDataLength()) {
      uint32_t ThreadID;
      char const *ThreadIDPtr = Data.data();
      memcpy(reinterpret_cast<char *>(&ThreadID), ThreadIDPtr, sizeof(ThreadID));
      
      uint64_t ThreadTime;
      char const *ThreadTimePtr = ThreadIDPtr + sizeof(ThreadID);
      memcpy(reinterpret_cast<char *>(&ThreadTime), ThreadTimePtr, sizeof(ThreadTime));
      
      uint64_t Signal;
      char const *SignalPtr = ThreadTimePtr + sizeof(ThreadTime);
      memcpy(reinterpret_cast<char *>(&Signal), SignalPtr, sizeof(Signal));
      
      char const *Name = SignalPtr + sizeof(Signal);
      auto const NameLength = strnlen(Name, getMaxNameLength());
      
      char const *Message = Name + NameLength + 1;
      
      assert(Data.end() >= Message);
      auto const MessageMaxLength = size_t(Data.end() - Message);
      auto const MessageLength = strnlen(Message, MessageMaxLength);
      
      if (NameLength < getMaxNameLength() && MessageLength < MessageMaxLength)
      {
        RetVal = CaughtSignalInfo(ThreadID, ThreadTime, Signal, Name, Message);
      }
    }
  }
  
  return RetVal;
}

// Note that this function must be as safe as possible to call from a signal
// handler. We should only call async signal safe functions.
//
void writeSignalInfo(OutputStreamAllocator &OutAllocator,
                     uint32_t ThreadID,
                     uint64_t ThreadTime,
                     int Signal,
                     char const * const Name,
                     char const * const Message)
{
  char OutData[getOutDataLength()];
  auto OutBlock =
    OutAllocator.getOutputBlock(BlockType::SignalInfo,
                                OutputBlock::getHeaderSize() + sizeof(OutData));
  
  if (OutBlock) {
    memset(OutData, '\0', sizeof(OutData));
    auto const OutDataEnd = OutData + sizeof(OutData);
    
    // Insert the thread ID.
    auto const ThreadIDPtr = OutData;
    memcpy(ThreadIDPtr,
           reinterpret_cast<char const *>(&ThreadID), sizeof(ThreadID));
    
    // Insert the thread time.
    auto const ThreadTimePtr = ThreadIDPtr + sizeof(ThreadID);
    memcpy(ThreadTimePtr,
           reinterpret_cast<char const *>(&ThreadTime), sizeof(ThreadTime));
    
    // Insert the signal number.
    uint64_t const SignalFixedWidth = Signal;
    auto const SignalPtr = ThreadTimePtr + sizeof(ThreadTime);
    memcpy(SignalPtr,
           reinterpret_cast<char const *>(&SignalFixedWidth),
           sizeof(SignalFixedWidth));
    
    // Insert the signal name.
    auto const NamePtr = SignalPtr + sizeof(SignalFixedWidth);
    strncpy(NamePtr, Name, getMaxNameLength());
    NamePtr[getMaxNameLength() - 1] = '\0';
    
    // Insert the signal description.
    auto const MessagePtr = NamePtr + strlen(NamePtr) + 1;
    strncpy(MessagePtr, Message, OutDataEnd - MessagePtr);
    OutData[sizeof(OutData)-1] = '\0';
    
    auto Offset = OutBlock->write(OutData, sizeof(OutData));
  }
}

} // namespace trace (in seec)

} // namespace seec
