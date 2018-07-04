//===- include/seec/Trace/TraceSignalInfo.hpp ----------------------- C++ -===//
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

#ifndef SEEC_TRACE_TRACESIGNALINFO_HPP
#define SEEC_TRACE_TRACESIGNALINFO_HPP

#include "llvm/ADT/Optional.h"

#include "seec/Util/IndexTypes.hpp"

namespace seec {

namespace trace {

class InputBlock;
class OutputStreamAllocator;

class CaughtSignalInfo
{
public:
  static llvm::Optional<CaughtSignalInfo> readFrom(InputBlock const &FromBlock);
  
  llvm::Optional<ThreadIDTy> getThreadID() const {
    llvm::Optional<ThreadIDTy> retVal;
    
    if (m_ThreadID) {
      retVal = ThreadIDTy(m_ThreadID - 1);
    }

    return retVal;
  }
  
  llvm::Optional<uint64_t> getThreadTime() const {
    llvm::Optional<uint64_t> retVal;
    
    if (m_ThreadTime) {
      retVal = m_ThreadTime;
    }
    
    return retVal;
  }
  
  int getSignal() const { return m_Signal; }
  
  char const *getName() const { return m_Name; }
  
  char const *getMessage() const { return m_Message; }
  
private:
  CaughtSignalInfo(uint32_t ThreadID,
                   uint64_t ThreadTime,
                   int Signal,
                   char const *Name,
                   char const *Message)
  : m_ThreadID(ThreadID),
    m_ThreadTime(ThreadTime),
    m_Signal(Signal),
    m_Name(Name),
    m_Message(Message)
  {}
  
  uint32_t m_ThreadID;
  uint64_t m_ThreadTime;
  int m_Signal;
  char const *m_Name;
  char const *m_Message;
};

void writeSignalInfo(OutputStreamAllocator &Out,
                     uint32_t ThreadID,
                     uint64_t ThreadTime,
                     int Signal,
                     char const *Name,
                     char const *Message);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_TRACESIGNALINFO_HPP
