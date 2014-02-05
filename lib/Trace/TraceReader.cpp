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
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Serialization.hpp"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <vector>

namespace seec {

namespace trace {


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
deserializeRuntimeError(EventRange Records) {
  using namespace seec::runtime_errors;
  
  auto NextRecord = Records.begin();
  if (NextRecord->getType() != EventType::RuntimeError)
    return std::make_pair(nullptr, NextRecord);
  
  auto &ErrorRecord = NextRecord.get<EventType::RuntimeError>();
  auto ErrorType = static_cast<RunErrorType>(ErrorRecord.getErrorType());
  
  ++NextRecord;
  
  // Read arguments.
  std::vector<std::unique_ptr<Arg>> Args;
  
  for (auto Count = ErrorRecord.getArgumentCount(); Count != 0; --Count) {
    auto &ArgRecord = NextRecord.get<EventType::RuntimeErrorArgument>();

    Args.emplace_back(deserializeRuntimeErrorArg(ArgRecord.getArgumentType(),
                                                 ArgRecord.getArgumentData()));
    
    ++NextRecord;
  }
  
  // Read additional errors.
  std::vector<std::unique_ptr<RunError>> Additional;
  
  for (auto Count = ErrorRecord.getAdditionalCount(); Count != 0; --Count) {
    auto Read = deserializeRuntimeError(rangeAfterIncluding(Records,
                                                            NextRecord));
    
    // If we couldn't read one of the additionals, the deserialization failed.
    if (!Read.first)
      return std::make_pair(nullptr, NextRecord);
    
    // Otherwise add the additional and continue from the first record that
    // follows the additional.
    Additional.emplace_back(std::move(Read.first));
    NextRecord = Read.second;
  }
  
  // Create the complete RunError.
  auto Error = new RunError(ErrorType,
                            std::move(Args),
                            std::move(Additional));
  
  return std::make_pair(std::unique_ptr<seec::runtime_errors::RunError>(Error),
                        NextRecord);
}


//------------------------------------------------------------------------------
// FunctionTrace
//------------------------------------------------------------------------------

llvm::ArrayRef<offset_uint> FunctionTrace::getChildList() const {
  auto List = *reinterpret_cast<offset_uint const *>(Data + ChildListOffset());
  return Thread->getOffsetList(List);
}

llvm::raw_ostream &operator<< (llvm::raw_ostream &Out, FunctionTrace const &T) {
  Out << "[Function Idx=" << T.getIndex()
      << ", [" << T.getThreadTimeEntered()
      << "," << T.getThreadTimeExited()
      << "] Children=" << T.getChildList().size()
      << "]";
  return Out;
}


//------------------------------------------------------------------------------
// ThreadTrace
//------------------------------------------------------------------------------

seec::Maybe<FunctionTrace>
ThreadTrace::getFunctionContaining(EventReference EvRef) const {
  auto Evs = rangeBefore(events(), EvRef);

  // Search backwards until we find the FunctionStart for the function that
  // contains EvRef.
  for (EventReference It(--Evs.end()); ; --It) {
    if (It->getType() == EventType::FunctionStart) {
      // This function must be the containing function, because we have skipped
      // all child functions.
      auto const &StartEv = It.get<EventType::FunctionStart>();
      return getFunctionTrace(StartEv.getRecord());
    }
    else if (It->getType() == EventType::FunctionEnd) {
      // Skip events in child function invocations.

      auto const &EndEv = It.get<EventType::FunctionEnd>();
      auto const Info = getFunctionTrace(EndEv.getRecord());
      It = Evs.referenceToOffset(Info.getEventStart());

      // It will be decremented at the end of the loop, correctly skipping the
      // FunctionStart event for this child function.
    }

    if (It == Evs.begin())
      break;
  }

  return seec::Maybe<FunctionTrace>();
}

uint64_t ThreadTrace::getFinalThreadTime() const {
  auto MaybeTime = lastSuccessfulApply(events(),
                    [this]
                    (EventRecordBase const &Ev) -> seec::Maybe<uint64_t>
                    {
                      auto Ty = Ev.getType();

                      if (Ty == EventType::FunctionEnd) {
                        auto EndEv = Ev.as<EventType::FunctionEnd>();
                        auto Record = EndEv.getRecord();
                        auto FTrace = this->getFunctionTrace(Record);
                        auto Exited = FTrace.getThreadTimeExited();
                        // Function might never have been exited, in which case
                        // it will have a zero exit time.
                        return Exited ? Exited : seec::Maybe<uint64_t>();
                      }
                      else if (Ty == EventType::FunctionStart) {
                        auto StartEv = Ev.as<EventType::FunctionStart>();
                        auto Record = StartEv.getRecord();
                        auto FTrace = this->getFunctionTrace(Record);
                        return FTrace.getThreadTimeEntered();
                      }

                      return Ev.getThreadTime();
                    });

  if (MaybeTime.assigned())
    return MaybeTime.get<0>();

  return 0;
}


//------------------------------------------------------------------------------
// ProcessTrace
//------------------------------------------------------------------------------

ProcessTrace::ProcessTrace(std::unique_ptr<InputBufferAllocator> WithAllocator,
                           std::unique_ptr<llvm::MemoryBuffer> Trace,
                           std::unique_ptr<llvm::MemoryBuffer> Data,
                           std::string ModuleIdentifier,
                           uint32_t NumThreads,
                           uint64_t FinalProcessTime,
                           std::vector<uintptr_t> GVAddresses,
                           std::vector<offset_uint> GVInitialData,
                           std::vector<uintptr_t> FAddresses,
                           std::vector<uintptr_t> WithStreamsInitial,
                           std::vector<std::unique_ptr<ThreadTrace>> TTraces
                           )
: Allocator(std::move(WithAllocator)),
  Trace(std::move(Trace)),
  Data(std::move(Data)),
  ModuleIdentifier(std::move(ModuleIdentifier)),
  NumThreads(NumThreads),
  FinalProcessTime(FinalProcessTime),
  GlobalVariableAddresses(std::move(GVAddresses)),
  GlobalVariableInitialData(std::move(GVInitialData)),
  FunctionAddresses(std::move(FAddresses)),
  StreamsInitial(std::move(WithStreamsInitial)),
  ThreadTraces(std::move(TTraces))
{}

seec::Maybe<std::unique_ptr<ProcessTrace>, seec::Error>
ProcessTrace::readFrom(std::unique_ptr<InputBufferAllocator> Allocator)
{
  auto TraceBuffer = Allocator->getProcessData(ProcessSegment::Trace);
  if (TraceBuffer.assigned<seec::Error>())
    return std::move(TraceBuffer.get<seec::Error>());
  
  auto DataBuffer = Allocator->getProcessData(ProcessSegment::Data);
  if (DataBuffer.assigned<seec::Error>())
    return std::move(DataBuffer.get<seec::Error>());
  
  BinaryReader TraceReader(TraceBuffer.get<0>()->getBufferStart(),
                           TraceBuffer.get<0>()->getBufferEnd());

  uint64_t Version = 0;
  TraceReader >> Version;

  if (Version != seec::trace::formatVersion()) {
    using namespace seec;
    
    auto const Expected = seec::trace::formatVersion();
    
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors",
                                           "ProcessTraceVersionIncorrect"},
                                          std::make_pair("version_found",
                                                         int64_t(Version)),
                                          std::make_pair("version_expected",
                                                         int64_t(Expected))));
  }

  std::string ModuleIdentifier;
  uint32_t NumThreads;
  uint64_t FinalProcessTime;
  std::vector<uintptr_t> GlobalVariableAddresses;
  std::vector<offset_uint> GlobalVariableInitialData;
  std::vector<uintptr_t> FunctionAddresses;
  std::vector<uintptr_t> StreamsInitial;
  std::vector<std::unique_ptr<ThreadTrace>> ThreadTraces;

  TraceReader >> ModuleIdentifier
              >> NumThreads
              >> FinalProcessTime
              >> GlobalVariableAddresses
              >> GlobalVariableInitialData
              >> FunctionAddresses
              >> StreamsInitial;

  if (TraceReader.error()) {
    using namespace seec;
    return Error(LazyMessageByRef::create("Trace",
                                          {"errors", "ProcessTraceFailRead"}));
  }

  for (uint32_t i = 0; i < NumThreads; ++i) {
    ThreadTraces.emplace_back(new ThreadTrace(*Allocator, i + 1));
  }

  return std::unique_ptr<ProcessTrace>(
            new ProcessTrace(std::move(Allocator),
                             std::move(TraceBuffer.get<0>()),
                             std::move(DataBuffer.get<0>()),
                             std::move(ModuleIdentifier),
                             NumThreads,
                             FinalProcessTime,
                             std::move(GlobalVariableAddresses),
                             std::move(GlobalVariableInitialData),
                             std::move(FunctionAddresses),
                             std::move(StreamsInitial),
                             std::move(ThreadTraces)));
}

seec::Maybe<std::vector<TraceFile>, seec::Error>
ProcessTrace::getAllFileData() const
{
  std::vector<TraceFile> Files;
  
  // Get the module.
  auto MaybeModule = Allocator->getModuleFile();
  if (MaybeModule.assigned<seec::Error>())
    return MaybeModule.move<seec::Error>();
  Files.emplace_back(MaybeModule.move<TraceFile>());
  
  // Get the process files.
  auto MaybeProcessTrace = Allocator->getProcessFile(ProcessSegment::Trace);
  if (MaybeProcessTrace.assigned<seec::Error>())
    return MaybeProcessTrace.move<seec::Error>();
  Files.emplace_back(MaybeProcessTrace.move<TraceFile>());
  
  auto MaybeProcessData = Allocator->getProcessFile(ProcessSegment::Data);
  if (MaybeProcessData.assigned<seec::Error>())
    return MaybeProcessData.move<seec::Error>();
  Files.emplace_back(MaybeProcessData.move<TraceFile>());
  
  // Get all threads' files.
  for (uint32_t i = 1; i <= NumThreads; ++i) {
    auto MaybeTrace = Allocator->getThreadFile(i, ThreadSegment::Trace);
    if (MaybeTrace.assigned<seec::Error>())
      return MaybeTrace.move<seec::Error>();
    Files.emplace_back(MaybeTrace.move<TraceFile>());
    
    auto MaybeEvents = Allocator->getThreadFile(i, ThreadSegment::Events);
    if (MaybeEvents.assigned<seec::Error>())
      return MaybeEvents.move<seec::Error>();
    Files.emplace_back(MaybeEvents.move<TraceFile>());
  }
  
  return std::move(Files);
}

Maybe<uint32_t>
ProcessTrace::getIndexOfFunctionAt(uintptr_t const Address) const
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
  return Trace.events().referenceToOffset(Ev.getOffset());
}


} // namespace trace (in seec)

} // namespace seec
