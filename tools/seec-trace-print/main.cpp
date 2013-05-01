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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Clang/MappedAllocaState.hpp"
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
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/system_error.h"

#include "unicode/unistr.h"

#include <array>
#include <memory>

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
  ShowRawEvents("R", cl::desc("show raw events"));

  static cl::opt<bool>
  ShowStates("S", cl::desc("show recreated states"));

  static cl::opt<bool>
  ShowErrors("E", cl::desc("show run-time errors"));
}

// From clang's driver.cpp:
llvm::sys::Path GetExecutablePath(const char *ArgV0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return llvm::sys::Path(ArgV0);

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void *) (intptr_t) GetExecutablePath;
  return llvm::sys::Path::GetMainExecutable(ArgV0, P);
}

void WriteDot(std::shared_ptr<seec::cm::Value const> Value,
              llvm::raw_fd_ostream &Stream)
{
  if (Value->isCompletelyInitialized()) {
    Stream << Value->getValueAsStringShort();
  }
  else {
    Stream << "uninitialized";
  }
}

void WriteDot(seec::cm::AllocaState const &State,
              llvm::raw_fd_ostream &Stream)
{
  Stream << "{ " << State.getDecl()->getNameAsString() << " | ";
  WriteDot(State.getValue(), Stream);
  Stream << " }";
}

void WriteDot(seec::cm::FunctionState const &State,
              llvm::raw_fd_ostream &Stream,
              std::vector<std::string> &FunctionNodes)
{
  std::string NodeIdentifier;
  
  {
    llvm::raw_string_ostream NodeIdentifierStream {NodeIdentifier};
    NodeIdentifierStream << "function" << reinterpret_cast<uintptr_t>(&State);
  }
  
  Stream << NodeIdentifier << " [";
  Stream << " label = \"{";
  Stream << State.getNameAsString();
  Stream << " | ";
  
  for (auto const &Parameter : State.getParameters())
    WriteDot(Parameter, Stream);
  
  for (auto const &Local : State.getLocals())
    WriteDot(Local, Stream);
  
  Stream << "}\" ";
  Stream << "];\n";
  
  FunctionNodes.emplace_back(std::move(NodeIdentifier));
}

void WriteDot(seec::cm::ThreadState const &State,
              llvm::raw_fd_ostream &Stream)
{
  auto const ID = State.getUnmappedState().getTrace().getThreadID();
  std::vector<std::string> FunctionNodes;
  
  Stream << "subgraph thread" << ID << " {\n";
  Stream << "node [shape = record];\n";
  
  for (auto const &FunctionState : State.getCallStack()) {
    WriteDot(FunctionState, Stream, FunctionNodes);
  }
  
  Stream << "{ rank=same; ";
  for (auto const &FunctionNode : FunctionNodes)
    Stream << FunctionNode << "; ";
  Stream << "};\n";
  
  Stream << "}\n";
}

void WriteDotGraph(seec::cm::ProcessState const &State,
                   char const *Filename)
{
  assert(Filename && "NULL Filename.");
  
  std::string StreamError;
  llvm::raw_fd_ostream Stream {Filename, StreamError};
  
  if (!StreamError.empty()) {
    llvm::errs() << "Error opening dot file: " << StreamError << "\n";
    return;
  }
  
  Stream << "digraph Process {\n";
  
  Stream << "rankdir=LR;\n";
  
  for (std::size_t i = 0; i < State.getThreadCount(); ++i) {
    WriteDot(State.getThread(i), Stream);
  }
  
  Stream << "}\n";
}

void PrintClangMapped(llvm::sys::Path const &ExecutablePath)
{
  // Attempt to setup the trace reader.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(InputDirectory);
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = std::move(MaybeIBA.get<seec::Error>());
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  assert(MaybeIBA.assigned<seec::trace::InputBufferAllocator>());
  auto BufferAllocator = MaybeIBA.get<seec::trace::InputBufferAllocator>();

  // Read the trace.
  auto CMProcessTraceLoad
    = cm::ProcessTrace::load(ExecutablePath.str(),
                             seec::makeUnique<trace::InputBufferAllocator>
                                             (std::move(BufferAllocator)));
  
  if (CMProcessTraceLoad.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = std::move(CMProcessTraceLoad.get<seec::Error>());
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  auto CMProcessTrace = std::move(CMProcessTraceLoad.get<0>());
  
  // Print the states.
  if (ShowStates) {
    seec::cm::ProcessState State(*CMProcessTrace);
    
    // If we're going to output dot graph files for the states, then setup the
    // output directory now.
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
    
    if (State.getThreadCount() == 1) {
      llvm::outs() << "Using thread-level iterator.\n";
      
      do {
        // Write textual description to stdout.
        llvm::outs() << State;
        llvm::outs() << "\n";
        
        // If enabled, write graphs in dot format.
        if (!OutputForDot.empty()) {
          // Add filename for this state.
          FilenameString.clear();
          FilenameStream << "state." << StateNumber++ << ".dot";
          FilenameStream.flush();
          
          llvm::sys::path::append(OutputForDot, FilenameString);
          
          // Write the graph.
          WriteDotGraph(State, OutputForDot.c_str());
          
          // Remove filename for this state.
          llvm::sys::path::remove_filename(OutputForDot);
        }
      } while (seec::cm::moveForward(State.getThread(0)));
    }
    else {
      llvm::outs() << "Using process-level iteration.\n";
      llvm::outs() << State;
    }
  }
}

void PrintUnmapped(llvm::sys::Path const &ExecutablePath)
{
  auto &Context = llvm::getGlobalContext();

  // Attempt to setup the trace reader.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(InputDirectory);
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = std::move(MaybeIBA.get<seec::Error>());
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  assert(MaybeIBA.assigned<seec::trace::InputBufferAllocator>());
  auto BufferAllocator = MaybeIBA.get<seec::trace::InputBufferAllocator>();
  
  // Attempt to read the trace.
  auto MaybeProcTrace = trace::ProcessTrace::readFrom(BufferAllocator);
  if (MaybeProcTrace.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = std::move(MaybeProcTrace.get<seec::Error>());
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  std::shared_ptr<trace::ProcessTrace> Trace(MaybeProcTrace.get<0>().release());

  // Load the bitcode.
  auto MaybeMod = BufferAllocator.getModule(Context);
  if (MaybeMod.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Error = std::move(MaybeMod.get<seec::Error>());
    llvm::errs() << Error.getMessage(Status, Locale()) << "\n";
    exit(EXIT_FAILURE);
  }
  
  assert(MaybeMod.assigned<llvm::Module *>());
  auto Mod = MaybeMod.get<llvm::Module *>();

  // Index the llvm::Module.
  auto ModIndexPtr = std::make_shared<seec::ModuleIndex>(*Mod, true);

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
  seec::seec_clang::MappedModule MapMod(*ModIndexPtr,
                                        ExecutablePath.str(),
                                        Diagnostics);

  // Print the raw events from each thread trace.
  if (ShowRawEvents) {
    auto NumThreads = Trace->getNumThreads();

    outs() << "Showing raw events:\n";

    for (uint32_t i = 1; i <= NumThreads; ++i) {
      auto &&Thread = Trace->getThreadTrace(i);

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

    while (ProcState.getProcessTime() != 0) {
      moveBackward(ProcState);
      outs() << ProcState << "\n";
    }
  }

  // Print basic descriptions of all run-time errors.
  if (ShowErrors) {
    clang::LangOptions LangOpt;

    clang::PrintingPolicy PrintPolicy(LangOpt);
    PrintPolicy.ConstantArraySizeAsWritten = true;

    auto NumThreads = Trace->getNumThreads();

    for (uint32_t i = 1; i <= NumThreads; ++i) {
      auto &&Thread = Trace->getThreadTrace(i);
      std::vector<uint32_t> FunctionStack;

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
              DescriptionPrinterUnicode Printer(std::move(MaybeDesc.get<0>()),
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
          auto const Prev = trace::rfind<trace::EventType::PreInstruction>
                                 (rangeBefore(Thread.events(), Ev));
          assert(Prev.assigned());

          auto const InstrIndex = Prev.get<0>()->getIndex();
          assert(InstrIndex.assigned());

          auto const FunIndex =
            ModIndexPtr->getFunctionIndex(FunctionStack.back());
          assert(FunIndex);

          auto const Instr = FunIndex->getInstruction(InstrIndex.get<0>());
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
                 << " Column " << Column << ":\n";

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

  llvm::sys::Path ExecutablePath = GetExecutablePath(argv[0], true);

  // Setup resource loading.
  ResourceLoader Resources(ExecutablePath);
  
  std::array<char const *, 3> ResourceList {
    {"RuntimeErrors", "SeeCClang", "Trace"}
  };
  
  if (!Resources.loadResources(ResourceList)) {
    llvm::errs() << "failed to load resources\n";
    exit(EXIT_FAILURE);
  }

  if (UseClangMapping) {
    PrintClangMapped(ExecutablePath);
  }
  else {
    PrintUnmapped(ExecutablePath);
  }
  
  return EXIT_SUCCESS;
}
