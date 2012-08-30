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
  using namespace seec::runtime_errors;
  using namespace seec::runtime_errors::format_selects;

  switch (Type) {
    case static_cast<uint8_t>(ArgType::Address):
      return std::unique_ptr<Arg>(new ArgAddress(Data));
    case static_cast<uint8_t>(ArgType::Object):
      return std::unique_ptr<Arg>(new ArgObject());
    case static_cast<uint8_t>(ArgType::Select):
      {
        uint32_t SelectType = Data >> 32; // Upper 32 bits
        uint32_t SelectValue = Data & 0xFFFFFFFF; // Lower 32 bits
        return createArgSelect(static_cast<SelectID>(SelectType), SelectValue);
      }
    case static_cast<uint8_t>(ArgType::Size):
      return std::unique_ptr<Arg>(new ArgSize(Data));
    default:
      llvm_unreachable("Unknown Type");
      return nullptr;
  }
}

std::unique_ptr<seec::runtime_errors::RunError>
deserializeRuntimeError(EventRange Records) {
  auto Record = Records.begin();

  if (Record->getType() != EventType::RuntimeError)
    return nullptr;

  auto &ErrorRecord = Record.get<EventType::RuntimeError>();
  auto ErrorType =
        static_cast<runtime_errors::RunErrorType>(ErrorRecord.getErrorType());

  std::vector<std::unique_ptr<seec::runtime_errors::Arg>> Args;

  for (auto Count = ErrorRecord.getArgumentCount(); Count != 0; --Count) {
    ++Record;

    auto &ArgRecord = Record.get<EventType::RuntimeErrorArgument>();

    Args.emplace_back(deserializeRuntimeErrorArg(ArgRecord.getArgumentType(),
                                                 ArgRecord.getArgumentData()));
  }

  return std::unique_ptr<seec::runtime_errors::RunError>(
          new seec::runtime_errors::RunError(ErrorType, std::move(Args)));
}


//------------------------------------------------------------------------------
// FunctionTrace
//------------------------------------------------------------------------------

llvm::ArrayRef<offset_uint> FunctionTrace::getChildList() const {
  auto List = *reinterpret_cast<offset_uint const *>(Data + ChildListOffset());
  return Thread->getOffsetList(List);
}

llvm::ArrayRef<offset_uint> FunctionTrace::getNonLocalChangeList() const {
  auto List
    = *reinterpret_cast<offset_uint const *>(Data + NonLocalChangeListOffset());
  return Thread->getOffsetList(List);
}

llvm::raw_ostream &operator<< (llvm::raw_ostream &Out, FunctionTrace const &T) {
  Out << "[Function Idx=" << T.getIndex()
      << ", [" << T.getThreadTimeEntered()
      << "," << T.getThreadTimeExited()
      << "] Children=" << T.getChildList().size()
      << ", NLChanges=" << T.getNonLocalChangeList().size()
      << "]";
  return Out;
}


//------------------------------------------------------------------------------
// ThreadTrace
//------------------------------------------------------------------------------

seec::util::Maybe<FunctionTrace>
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

  return seec::util::Maybe<FunctionTrace>();
}

uint64_t ThreadTrace::getFinalThreadTime() const {
  auto MaybeTime = lastSuccessfulApply(events(),
                    [this]
                    (EventRecordBase const &Ev) -> seec::util::Maybe<uint64_t>
                    {
                      auto Ty = Ev.getType();
                      
                      if (Ty == EventType::FunctionEnd) {
                        auto EndEv = Ev.as<EventType::FunctionEnd>();
                        auto Record = EndEv.getRecord();
                        auto FTrace = this->getFunctionTrace(Record);
                        auto Exited = FTrace.getThreadTimeExited();
                        // Function might never have been exited, in which case
                        // it will have a zero exit time.
                        return Exited ? Exited : seec::util::Maybe<uint64_t>();
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

ProcessTrace::ProcessTrace(InputBufferAllocator &Allocator,
                           std::unique_ptr<llvm::MemoryBuffer> &&Trace,
                           std::unique_ptr<llvm::MemoryBuffer> &&Data,
                           uint64_t Version,
                           std::string &&ModuleIdentifier,
                           uint32_t NumThreads,
                           uint64_t FinalProcessTime,
                           std::vector<uint64_t> &&GVAddresses,
                           std::vector<offset_uint> &&GVInitialData,
                           std::vector<uint64_t> &&FAddresses,
                           std::vector<std::unique_ptr<ThreadTrace>> &&TTraces
                           )
: BufferAllocator(Allocator),
  Trace(std::move(Trace)),
  Data(std::move(Data)),
  Version(Version),
  ModuleIdentifier(ModuleIdentifier),
  NumThreads(NumThreads),
  FinalProcessTime(FinalProcessTime),
  GlobalVariableAddresses(GVAddresses),
  GlobalVariableInitialData(GVInitialData),
  FunctionAddresses(FAddresses),
  ThreadTraces(std::move(TTraces))
{}

seec::util::Maybe<std::unique_ptr<ProcessTrace>,
                  std::unique_ptr<seec::Error>>
ProcessTrace::readFrom(InputBufferAllocator &Allocator) {
  auto TraceBuffer = Allocator.getProcessData(ProcessSegment::Trace);
  auto DataBuffer = Allocator.getProcessData(ProcessSegment::Data);

  BinaryReader TraceReader(TraceBuffer->getBufferStart(),
                           TraceBuffer->getBufferEnd());

  uint64_t Version = 0;
  TraceReader >> Version;

  if (Version != seec::trace::formatVersion()) {
    return makeUnique<seec::Error>();
  }

  std::string ModuleIdentifier;
  uint32_t NumThreads;
  uint64_t FinalProcessTime;
  std::vector<uint64_t> GlobalVariableAddresses;
  std::vector<offset_uint> GlobalVariableInitialData;
  std::vector<uint64_t> FunctionAddresses;
  std::vector<std::unique_ptr<ThreadTrace>> ThreadTraces;

  TraceReader >> ModuleIdentifier
              >> NumThreads
              >> FinalProcessTime
              >> GlobalVariableAddresses
              >> GlobalVariableInitialData
              >> FunctionAddresses;

  if (TraceReader.error()) {
    return makeUnique<seec::Error>();
  }

  for (uint32_t i = 0; i < NumThreads; ++i) {
    ThreadTraces.emplace_back(new ThreadTrace(Allocator, i + 1));
  }

  return std::unique_ptr<ProcessTrace>(
            new ProcessTrace(Allocator,
                             std::move(TraceBuffer),
                             std::move(DataBuffer),
                             Version,
                             std::move(ModuleIdentifier),
                             NumThreads,
                             FinalProcessTime,
                             std::move(GlobalVariableAddresses),
                             std::move(GlobalVariableInitialData),
                             std::move(FunctionAddresses),
                             std::move(ThreadTraces)));
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
