//===- tools/seec-instrument/seec-instrument.cpp --------------------------===//
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

#include "seec/Transforms/RecordExternal/RecordExternal.hpp"

#include "llvm/DataLayout.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetLibraryInfo.h"

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"), cl::init("-"),
              cl::value_desc("filename"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Override output filename"),
               cl::value_desc("filename"));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));


int main(int argc, char **argv)
{
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.
  LLVMContext &Context = getGlobalContext();

  cl::ParseCommandLineOptions(argc, argv,
    "seec llvm module instrumentation\n");

  SMDiagnostic Err;

  // Load the input module...
  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));

  if (M.get() == 0) {
    Err.print(argv[0], errs());
    return 1;
  }

  // TODO: don't delete output file
  // Figure out what stream we are supposed to write to...
  OwningPtr<tool_output_file> Out;

  // Default to standard output.
  if (OutputFilename.empty())
    OutputFilename = "-";

  std::string ErrorInfo;
  Out.reset(new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                                 raw_fd_ostream::F_Binary));
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    return 1;
  }

  // Build the PassManager
  PassManager Passes;

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  TargetLibraryInfo *TLI = new TargetLibraryInfo(Triple(M->getTargetTriple()));
  Passes.add(TLI);

  // Add an appropriate DataLayout instance for this module.
  DataLayout *DL = 0;
  const std::string &ModuleDataLayout = M.get()->getDataLayout();
  if (!ModuleDataLayout.empty())
    DL = new DataLayout(ModuleDataLayout);
  if (DL)
    Passes.add(DL);

  // Add SeeC's recording instrumentation pass
  Passes.add(new InsertExternalRecording());

  // Verify the final module
  Passes.add(createVerifierPass());

  // Write the final module
  if (OutputAssembly)
    Passes.add(createPrintModulePass(&Out->os()));
  else
    Passes.add(createBitcodeWriterPass(Out->os()));

  // Run the passes
  Passes.run(*M.get());
  
  Out->keep();

  return 0;
}
