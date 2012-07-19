//===- main.cpp - SeeC Interpreter ----------------------------------------===//
//
// Based on lli.cpp from
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Trace/ExecutionListener.hpp"
#include "seec/Trace/ExecutionState.hpp"
#include "seec/Trace/TracePrinter.hpp"
#include "seec/Trace/TraceWriter.hpp"
#include "seec/Trace/TracingExecutionListener.hpp"
#include "seec/Transforms/RecordInternal/RecordInternal.hpp"
#include "seec/Transforms/RecordInternal/RecordPoints.hpp"
#include "seec/Util/InstrumentModule.hpp"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Type.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <cerrno>
#include <csetjmp>

#ifdef __CYGWIN__
#include <cygwin/version.h>
#if defined(CYGWIN_VERSION_DLL_MAJOR) && CYGWIN_VERSION_DLL_MAJOR<1007
#define DO_NOTHING_ATEXIT 1
#endif
#endif

using namespace llvm;
using namespace seec;
using namespace seec::trace;

namespace {
  cl::opt<std::string>
  InputFile(cl::desc("<input bitcode>"), cl::Positional, cl::init("-"));

  cl::list<std::string>
  InputArgv(cl::ConsumeAfter, cl::desc("<program arguments>..."));

  cl::opt<bool> UseMCJIT(
    "use-mcjit", cl::desc("Enable use of the MC-based JIT (if available)"),
    cl::init(false));

  cl::opt<std::string>
  MArch("march",
        cl::desc("Architecture to generate assembly for (see --version)"));

  cl::opt<std::string>
  MCPU("mcpu",
       cl::desc("Target a specific cpu type (-mcpu=help for details)"),
       cl::value_desc("cpu-name"),
       cl::init(""));

  cl::list<std::string>
  MAttrs("mattr",
         cl::CommaSeparated,
         cl::desc("Target specific attributes (-mattr=help for details)"),
         cl::value_desc("a1,+a2,-a3,..."));

  cl::opt<std::string>
  EntryFunc("entry-function",
            cl::desc("Specify the entry function (default = 'main') "
                     "of the executable"),
            cl::value_desc("function"),
            cl::init("main"));

  cl::opt<std::string>
  FakeArgv0("fake-argv0",
            cl::desc("Override the 'argv[0]' value passed into the executing"
                     " program"), cl::value_desc("executable"));

  cl::opt<bool>
  DisableCoreFiles("disable-core-files", cl::Hidden,
                   cl::desc("Disable emission of core files if possible"));

  cl::opt<Reloc::Model>
  RelocModel("relocation-model",
             cl::desc("Choose relocation model"),
             cl::init(Reloc::Default),
             cl::values(
            clEnumValN(Reloc::Default, "default",
                       "Target default relocation model"),
            clEnumValN(Reloc::Static, "static",
                       "Non-relocatable code"),
            clEnumValN(Reloc::PIC_, "pic",
                       "Fully relocatable, position independent code"),
            clEnumValN(Reloc::DynamicNoPIC, "dynamic-no-pic",
                       "Relocatable external references, non-relocatable code"),
            clEnumValEnd));

  cl::opt<llvm::CodeModel::Model>
  CMModel("code-model",
          cl::desc("Choose code model"),
          cl::init(CodeModel::JITDefault),
          cl::values(clEnumValN(CodeModel::JITDefault, "default",
                                "Target default JIT code model"),
                     clEnumValN(CodeModel::Small, "small",
                                "Small code model"),
                     clEnumValN(CodeModel::Kernel, "kernel",
                                "Kernel code model"),
                     clEnumValN(CodeModel::Medium, "medium",
                                "Medium code model"),
                     clEnumValN(CodeModel::Large, "large",
                                "Large code model"),
                     clEnumValEnd));

}

static ExecutionEngine *EE = 0;

static void do_shutdown() {
  // Cygwin-1.5 invokes DLL's dtors before atexit handler.
#ifndef DO_NOTHING_ATEXIT
  delete EE;
  llvm_shutdown();
#endif
}

//===----------------------------------------------------------------------===//
// main Driver function
//
int main(int argc, char **argv, char * const *envp) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  atexit(do_shutdown);  // Call llvm_shutdown() on exit.

  // If we have a native target, initialize it to ensure it is linked in and
  // usable by the JIT.
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  cl::ParseCommandLineOptions(argc, argv,
                              "seec llvm interpreter\n");

  // If the user doesn't want core files, disable them.
  if (DisableCoreFiles)
    sys::Process::PreventCoreFiles();

  // Load the bitcode...
  SMDiagnostic Err;
  Module *Mod = ParseIRFile(InputFile, Err, Context);
  if (!Mod) {
    Err.print(argv[0], errs());
    return 1;
  }

  // If not jitting lazily, load the whole bitcode file eagerly too.
  std::string ErrorMsg;
  if (Mod->MaterializeAllPermanently(&ErrorMsg)) {
    errs() << argv[0] << ": bitcode didn't read correctly.\n";
    errs() << "Reason: " << ErrorMsg << "\n";
    exit(1);
  }

  // Instrument the module
  InternalInstrumentation<TracingExecutionListener> Instrumentation(Mod);

  // Setup properties for the ExecutionEngine
  EngineBuilder builder(Mod);
  builder.setMArch(MArch);
  builder.setMCPU(MCPU);
  builder.setMAttrs(MAttrs);
  builder.setRelocationModel(RelocModel);
  builder.setCodeModel(CMModel);
  builder.setErrorStr(&ErrorMsg);
  builder.setEngineKind(EngineKind::JIT);

  // Enable MCJIT, if desired.
  if (UseMCJIT)
    builder.setUseMCJIT(true);

  builder.setOptLevel(CodeGenOpt::Default);

  EE = builder.create();
  if (!EE) {
    if (!ErrorMsg.empty())
      errs() << argv[0] << ": error creating EE: " << ErrorMsg << "\n";
    else
      errs() << argv[0] << ": unknown error creating EE!\n";
    exit(1);
  }

  EE->DisableLazyCompilation(true);

  // If the user specifically requested an argv[0] to pass into the program,
  // do it now.
  if (!FakeArgv0.empty()) {
    InputFile = FakeArgv0;
  } else {
    // Otherwise, if there is a .bc suffix on the executable strip it off, it
    // might confuse the program.
    if (StringRef(InputFile).endswith(".bc"))
      InputFile.erase(InputFile.length() - 3);
  }

  // Add the module's name to the start of the vector of arguments to main().
  InputArgv.insert(InputArgv.begin(), InputFile);

  // Call the main function from M as if its signature were:
  //   int main (int argc, char **argv, const char **envp)
  // using the contents of Args to determine argc & argv, and the contents of
  // EnvVars to determine envp.
  //
  Function *EntryFn = Mod->getFunction(EntryFunc);
  if (!EntryFn) {
    errs() << '\'' << EntryFunc << "\' function not found in module.\n";
    return -1;
  }

  // Reset errno to zero on entry to main.
  errno = 0;

  // Notify the ExecutionListener of the ExecutionEngine
  Instrumentation.executionListener()->notifyExecutionEngine(EE, Mod);

  // Run static constructors.
  EE->runStaticConstructorsDestructors(false);

  for (Module::iterator I = Mod->begin(), E = Mod->end(); I != E; ++I) {
    Function *Fn = &*I;
    if (Fn != EntryFn && !Fn->isDeclaration())
      EE->getPointerToFunction(Fn);
  }

  // Run main.
  int Result;

  // If the user's program exited using exit(), it's caught here
  if (setjmp(Instrumentation.internalListener()->ExitJump)) {
    Result = Instrumentation.internalListener()->getExitCode();
  }
  else {
    Result = EE->runFunctionAsMain(EntryFn, InputArgv, envp);

    // Run static destructors.
    EE->runStaticConstructorsDestructors(true);
  }

  outs() << "Execution finished with result: " << Result << "\n";

  auto Trace = Instrumentation.executionListener()->trace();

  // Print (test)
  outs() << "Trace collected:\n";
  TracePrinter TracePrint {outs()};
  TracePrint.visit(Trace);
  outs() << '\n';

  // Writer test
  raw_fd_ostream TraceOut ("trace.out", ErrorMsg, raw_fd_ostream::F_Binary);
  if (!ErrorMsg.empty()) {
    errs() << "Error creating trace.out: " << ErrorMsg << '\n';
    exit(EXIT_FAILURE);
  }

  auto OriginalModule = Instrumentation.originalModule();
  if (!writeTrace(OriginalModule, Trace, TraceOut, ErrorMsg)) {
    errs() << "Error writing trace.out: " << ErrorMsg << '\n';
  }

  TraceOut.close();

  exit(0);
}
