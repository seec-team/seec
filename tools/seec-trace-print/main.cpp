//===- tools/seec-trace-print/main.cpp ------------------------------------===//
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
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/ModuleIndex.hpp"
#include "seec/Util/Resources.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
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
#include "llvm/Support/system_error.h"

#include "unicode/unistr.h"

#include "OnlinePythonTutor.hpp"

#include <array>
#include <memory>
#include <type_traits>

using namespace seec;
using namespace llvm;

namespace {
  static cl::opt<std::string>
  InputDirectory(cl::desc("<input trace>"), cl::Positional, cl::init(""));

  static cl::opt<bool>
  UseClangMapping("C", cl::desc("use SeeC-Clang mapped states"));

  static cl::opt<std::string>
  OutputDirectoryForClangMappedDot("G", cl::desc("output dot graphs to this directory"));

  static cl::opt<bool>
  TestGraphGeneration("graph-test", cl::desc("generate dot graphs (but do not write them)"));

  static cl::opt<bool>
  ShowCounts("counts", cl::desc("show event counts"));

  static cl::opt<bool>
  ShowRawEvents("R", cl::desc("show raw events"));

  static cl::opt<bool>
  ShowStates("S", cl::desc("show recreated states"));

  static cl::opt<bool>
  ShowErrors("E", cl::desc("show run-time errors"));

  static cl::opt<bool>
  OnlinePythonTutor("P", cl::desc("output suitable for Online Python Tutor"));

  static cl::opt<bool>
  ReverseStates("reverse", cl::desc("show reverse iterated states at the end"));
}

// From clang's driver.cpp:
std::string GetExecutablePath(const char *Argv0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return Argv0;

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void*) (intptr_t) GetExecutablePath;
  return llvm::sys::fs::getMainExecutable(Argv0, P);
}

void WriteDotGraph(seec::cm::ProcessState const &State,
                   char const *Filename,
                   std::string const &DotString)
{
  assert(Filename && "NULL Filename.");
  
  std::string StreamError;
  llvm::raw_fd_ostream Stream {Filename, StreamError};
  
  if (!StreamError.empty()) {
    llvm::errs() << "Error opening dot file: " << StreamError << "\n";
    return;
  }
  
  Stream << DotString;
}

void PrintClangMappedStates(seec::cm::ProcessTrace const &Trace)
{
  seec::cm::ProcessState State(Trace);
  
  // If we're going to output dot graph files for the states, then setup the
  // output directory and layout handler now.
  std::unique_ptr<seec::cm::graph::LayoutHandler> LayoutHandler;
  llvm::SmallString<256> OutputForDot;
  std::string FilenameString;
  llvm::raw_string_ostream FilenameStream {FilenameString};
  long StateNumber = 1;
  
  if (!OutputDirectoryForClangMappedDot.empty()) {
    OutputForDot = OutputDirectoryForClangMappedDot;
    
    bool Existed;
    auto const Err =
      llvm::sys::fs::create_directories(llvm::StringRef(OutputForDot),
                                        Existed);
    
    if (Err != llvm::errc::success) {
      llvm::errs() << "Couldn't create output directory.\n";
      return;
    }
  }

  if (!OutputDirectoryForClangMappedDot.empty() || TestGraphGeneration) {
    LayoutHandler.reset(new seec::cm::graph::LayoutHandler());
    LayoutHandler->addBuiltinLayoutEngines();
  }
  
  if (State.getThreadCount() == 1) {
    llvm::outs() << "Using thread-level iterator.\n";
    auto const WriteGraphs = !OutputForDot.empty() || TestGraphGeneration;
    
    do {
      // Write textual description to stdout.
      llvm::outs() << State << "\n";
      
      // If enabled, write graphs in dot format.
      if (WriteGraphs) {
        auto const Layout = LayoutHandler->doLayout(State);

        if (!OutputForDot.empty()){
          // Add filename for this state.
          FilenameString.clear();
          FilenameStream << "state." << StateNumber++ << ".dot";
          FilenameStream.flush();

          llvm::sys::path::append(OutputForDot, FilenameString);

          // Write the graph.
          WriteDotGraph(State, OutputForDot.c_str(), Layout.getDotString());

          // Remove filename for this state.
          llvm::sys::path::remove_filename(OutputForDot);
        }
      }
    } while (seec::cm::moveForward(State.getThread(0)));

    if (ReverseStates) {
      while (seec::cm::moveBackward(State.getThread(0))) {
        llvm::outs() << State << "\n";
      }
    }
  }
  else {
    llvm::outs() << "Using process-level iteration.\n";
    llvm::outs() << State;
  }
}

void PrintClangMapped()
{
  // Attempt to setup the trace reader.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(InputDirectory);
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeIBA.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  auto IBA = seec::makeUnique<trace::InputBufferAllocator>
                             (MaybeIBA.move<trace::InputBufferAllocator>());

  // Read the trace.
  auto CMProcessTraceLoad = cm::ProcessTrace::load(std::move(IBA));
  
  if (CMProcessTraceLoad.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = CMProcessTraceLoad.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  auto CMProcessTrace = CMProcessTraceLoad.move<0>();
  
  if (ShowStates) {
    PrintClangMappedStates(*CMProcessTrace);
  }
  else if (OnlinePythonTutor) {
    PrintOnlinePythonTutor(*CMProcessTrace,
                           OPTSettings{}.setPyCrazyMode(true));
  }
}

void PrintUnmapped()
{
  auto &Context = llvm::getGlobalContext();

  // Attempt to setup the trace reader.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(InputDirectory);
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeIBA.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  auto IBA = seec::makeUnique<trace::InputBufferAllocator>
                             (MaybeIBA.move<trace::InputBufferAllocator>());
  
  // Load the bitcode.
  auto MaybeMod = IBA->getModule(Context);
  if (MaybeMod.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = MaybeMod.move<seec::Error>();
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  auto const Mod = MaybeMod.get<llvm::Module *>();
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
    outs() << ProcState << "\n";

    while (ProcState.getProcessTime() != Trace->getFinalProcessTime()) {
      moveForward(ProcState);
      outs() << ProcState << "\n";
    }

    if (ReverseStates) {
      while (ProcState.getProcessTime() != 0) {
        moveBackward(ProcState);
        outs() << ProcState << "\n";
      }
    }
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
            
            auto MaybeDesc = Description::create(*RunErr.first);
            
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

int main(int argc, char **argv, char * const *envp) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  atexit(llvm_shutdown);

  cl::ParseCommandLineOptions(argc, argv, "seec trace printer\n");

  auto const ExecutablePath = GetExecutablePath(argv[0], true);

  // Setup resource loading.
  auto const ResourcePath = seec::getResourceDirectory(ExecutablePath);
  ResourceLoader Resources(ResourcePath);
  
  std::array<char const *, 3> ResourceList {
    {"RuntimeErrors", "SeeCClang", "Trace"}
  };
  
  if (!Resources.loadResources(ResourceList)) {
    llvm::errs() << "failed to load resources\n";
    exit(EXIT_FAILURE);
  }

  if (UseClangMapping || OnlinePythonTutor) {
    PrintClangMapped();
  }
  else {
    PrintUnmapped();
  }
  
  return EXIT_SUCCESS;
}
