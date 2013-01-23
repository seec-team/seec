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
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedProcessState.hpp"
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

#include "llvm/LLVMContext.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
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


#if 0 // test clang-mapped trace interface.

  // Read the trace.
  auto CMProcessTraceLoad
    = cm::ProcessTrace::load(ExecutablePath.str(),
                             seec::makeUnique<trace::InputBufferAllocator>());
  
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
  }
  
#else // raw printing (not clang-mapped)

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
        outs() << "  " << Ev << "\n";
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
          assert(!FunctionStack.empty());
          // Print a textual description of the error.
          auto ErrRange = rangeAfterIncluding(Thread.events(), Ev);
          auto RunErr = deserializeRuntimeError(ErrRange);
          if (RunErr) {
            auto UniStr = seec::runtime_errors::format(*RunErr);
            outs() << "Error: \"" << UniStr << "\"\n";
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

#endif


  return EXIT_SUCCESS;
}
