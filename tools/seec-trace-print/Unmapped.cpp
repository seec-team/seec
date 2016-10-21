//===- tools/seec-trace-print/Unmapped.cpp --------------------------------===//
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

#include "seec/Clang/GraphLayout.hpp"
#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedStateMovement.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StateMovement.hpp"
#include "seec/Trace/TraceFormat.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Trace/TraceSearch.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Resources.hpp"
#include "seec/wxWidgets/AugmentResources.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"

#include "unicode/unistr.h"

#include "Unmapped.hpp"

#include <array>
#include <memory>
#include <system_error>
#include <type_traits>

using namespace seec;
using namespace llvm;

namespace seec {
  namespace trace_print {
    extern cl::opt<std::string> InputDirectory;

    extern cl::opt<bool> UseClangMapping;

    extern cl::opt<std::string> OutputDirectoryForClangMappedDot;

    extern cl::opt<bool> TestGraphGeneration;

    extern cl::opt<bool> ShowCounts;

    extern cl::opt<bool> ShowRawEvents;

    extern cl::opt<bool> ShowStates;

    extern cl::opt<bool> ShowErrors;

    extern cl::opt<bool> ReverseStates;

    extern cl::opt<bool> ShowComparable;

    extern cl::opt<bool> Quiet;

    extern cl::opt<bool> TestMovement;
  }
}

using namespace seec::trace_print;

void PrintUnmappedState(seec::trace::ProcessState const &State)
{
  if (Quiet)
    return;

  if (ShowComparable) {
    seec::trace::printComparable(outs(), State);
    outs() << "\n";
  }
  else {
    outs() << State << "\n";
  }
}

void PrintUnmapped(seec::AugmentationCollection const &Augmentations)
{
  llvm::LLVMContext Context{};

  // Attempt to setup the trace reader.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(InputDirectory);
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeIBA.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }

  auto IBA = llvm::make_unique<trace::InputBufferAllocator>
                              (MaybeIBA.move<trace::InputBufferAllocator>());

  // Load the bitcode.
  auto MaybeMod = IBA->getModule(Context);
  if (MaybeMod.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeMod.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }

  auto Mod = MaybeMod.move<std::unique_ptr<llvm::Module>>();
  auto ModIndexPtr = std::make_shared<seec::ModuleIndex>(*Mod, true);

  // Attempt to read the trace (this consumes the IBA).
  auto MaybeProcTrace = trace::ProcessTrace::readFrom(std::move(IBA));
  if (MaybeProcTrace.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeProcTrace.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }

  std::shared_ptr<trace::ProcessTrace> Trace(MaybeProcTrace.get<0>().release());

  if (ShowCounts) {
    using namespace seec::trace;

    auto const NumThreads = Trace->getNumThreads();

    // Count each EventType.
    typedef std::underlying_type<EventType>::type UnderlyingEventTypeTy;
    auto const Highest = static_cast<UnderlyingEventTypeTy>(EventType::Highest);
    uint64_t Counts[Highest];

    std::memset(Counts, 0, sizeof(Counts));

    for (uint32_t i = 1; i <= NumThreads; ++i) {
      auto const &Thread = Trace->getThreadTrace(i);

      for (auto const &Ev : Thread.events())
        ++Counts[static_cast<UnderlyingEventTypeTy>(Ev.getType())];
    }

    // Print the counts for each EventType.
    outs() << "EventType\tSize\tCount\tTotal\n";

#define SEEC_TRACE_EVENT(NAME, MEMBERS, TRAITS)                                \
    {                                                                          \
      auto const Index = static_cast<UnderlyingEventTypeTy>(EventType::NAME);  \
      auto const Size = sizeof(EventRecord<EventType::NAME>);                  \
      outs() << #NAME "\t" << Size << "\t" << Counts[Index] << "\t"            \
             << (Counts[Index] * Size) << "\n";                                \
    }
#include "seec/Trace/Events.def"
  }

  // Print the raw events from each thread trace.
  if (ShowRawEvents) {
    auto NumThreads = Trace->getNumThreads();

    outs() << "Showing raw events:\n";

    for (uint32_t i = 1; i <= NumThreads; ++i) {
      auto &&Thread = Trace->getThreadTrace(i);

      if (NumThreads > 1)
        outs() << "Thread #" << i << ":\n";

      outs() << "Functions:\n";

      for (auto Offset: Thread.topLevelFunctions()) {
        outs() << " @" << Offset << "\n";
        outs() << " " << Thread.getFunctionTrace(Offset) << "\n";
      }

      outs() << "Events:\n";

      for (auto &&Ev: Thread.events()) {
        if (Ev.isBlockStart())
          outs() << "\n";
        outs() << Ev << " @" << Thread.events().offsetOf(Ev) << "\n";
      }
    }
  }

  // Recreate complete process states and print the details.
  if (ShowStates) {
    outs() << "Recreating states:\n";

    trace::ProcessState ProcState{Trace, ModIndexPtr};
    PrintUnmappedState(ProcState);

    while (ProcState.getProcessTime() != Trace->getFinalProcessTime()) {
      moveForward(ProcState);
      PrintUnmappedState(ProcState);
    }

    if (ReverseStates) {
      while (ProcState.getProcessTime() != 0) {
        moveBackward(ProcState);
        PrintUnmappedState(ProcState);
      }
    }
  }

  // Test state movement only.
  if (TestMovement) {
    trace::ProcessState ProcState{Trace, ModIndexPtr};
    moveForwardUntil(ProcState,
                     [] (trace::ProcessState const &) { return false; });

    moveBackwardUntil(ProcState,
                      [] (trace::ProcessState const &) { return false; });
  }

  // Print basic descriptions of all run-time errors.
  if (ShowErrors) {
    // Setup diagnostics printing for Clang diagnostics.
    IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts
      = new clang::DiagnosticOptions();
    DiagOpts->ShowColors = true;

    clang::TextDiagnosticPrinter DiagnosticPrinter(errs(), &*DiagOpts);

    IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics
      = new clang::DiagnosticsEngine(
        IntrusiveRefCntPtr<clang::DiagnosticIDs>(new clang::DiagnosticIDs()),
        &*DiagOpts,
        &DiagnosticPrinter,
        false);

    Diagnostics->setSuppressSystemWarnings(true);
    Diagnostics->setIgnoreAllWarnings(true);

    // Setup the map to find Decls and Stmts from Instructions
    seec::seec_clang::MappedModule MapMod(*ModIndexPtr, Diagnostics);

    clang::LangOptions LangOpt;

    clang::PrintingPolicy PrintPolicy(LangOpt);
    PrintPolicy.ConstantArraySizeAsWritten = true;

    auto NumThreads = Trace->getNumThreads();

    for (uint32_t i = 1; i <= NumThreads; ++i) {
      auto &&Thread = Trace->getThreadTrace(i);
      std::vector<uint32_t> FunctionStack;

      if (NumThreads > 1)
        outs() << "Thread #" << i << ":\n";

      for (auto &&Ev: Thread.events()) {
        if (Ev.getType() == trace::EventType::FunctionStart) {
          auto const &Record = Ev.as<trace::EventType::FunctionStart>();
          auto const Info = Thread.getFunctionTrace(Record.getRecord());
          FunctionStack.push_back(Info.getIndex());
        }
        else if (Ev.getType() == trace::EventType::FunctionEnd) {
          assert(!FunctionStack.empty());

          FunctionStack.pop_back();
        }
        else if (Ev.getType() == trace::EventType::RuntimeError) {
          auto &EvRecord = Ev.as<trace::EventType::RuntimeError>();
          if (!EvRecord.getIsTopLevel())
            continue;

          assert(!FunctionStack.empty());

          // Print a textual description of the error.
          auto ErrRange = rangeAfterIncluding(Thread.events(), Ev);
          auto RunErr = deserializeRuntimeError(ErrRange);

          if (RunErr.first) {
            using namespace seec::runtime_errors;

            auto MaybeDesc = Description::create(*RunErr.first,
                                                 Augmentations.getCallbackFn());

            if (MaybeDesc.assigned(0)) {
              DescriptionPrinterUnicode Printer(MaybeDesc.move<0>(),
                                                "\n",
                                                "  ");

              llvm::outs() << Printer.getString() << "\n";
            }
            else if (MaybeDesc.assigned<seec::Error>()) {
              UErrorCode Status = U_ZERO_ERROR;
              llvm::errs() << MaybeDesc.get<seec::Error>()
                                       .getMessage(Status, Locale()) << "\n";
              exit(EXIT_FAILURE);
            }
            else {
              llvm::outs() << "Couldn't get error description.\n";
            }
          }

          // Find the Instruction responsible for this error.
          auto const MaybeInstrIndex = trace::lastSuccessfulApply(
            rangeBefore(Thread.events(), Ev),
            [] (trace::EventRecordBase const &Event) -> seec::Maybe<uint32_t> {
              if (Event.isInstruction())
                return Event.getIndex();
              return seec::Maybe<uint32_t>();
            });

          auto const InstrIndex = MaybeInstrIndex.get<uint32_t>();
          auto const FunIndex =
            ModIndexPtr->getFunctionIndex(FunctionStack.back());
          assert(FunIndex);

          auto const Instr = FunIndex->getInstruction(InstrIndex);
          assert(Instr);

          // Show the Clang Stmt that caused the error.
          auto const StmtAndAST = MapMod.getStmtAndMappedAST(Instr);
          assert(StmtAndAST.first && StmtAndAST.second);

          auto const &AST = StmtAndAST.second->getASTUnit();
          auto const &SrcManager = AST.getSourceManager();

          auto const LocStart = StmtAndAST.first->getLocStart();
          auto const Filename = SrcManager.getFilename(LocStart);
          auto const Line = SrcManager.getSpellingLineNumber(LocStart);
          auto const Column = SrcManager.getSpellingColumnNumber(LocStart);

          outs() << Filename
                 << ", Line " << Line
                 << " Column " << Column << ": ";

          StmtAndAST.first->printPretty(outs(),
                                        nullptr,
                                        PrintPolicy);

          outs() << "\n";
        }
      }
    }
  }
}
