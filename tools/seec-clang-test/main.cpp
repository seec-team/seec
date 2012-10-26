//===- main.cpp - SeeC Clang test -----------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Clang/Compile.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "llvm/Module.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"

#include <string>

using namespace llvm;
using namespace clang;
using namespace seec::seec_clang;

namespace {
  cl::opt<std::string>
  ModuleOutputFile("o",
                   cl::desc("File to write LLVM Module to"),
                   cl::init("a.ll"));

  cl::opt<std::string>
  InputFile(cl::desc("<input source>"), cl::Positional, cl::init("-"));
} // namespace anonymous

// from clang's driver.cpp
llvm::sys::Path GetExecutablePath(const char *ArgV0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return llvm::sys::Path(ArgV0);

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void*) (intptr_t) GetExecutablePath;
  return llvm::sys::Path::GetMainExecutable(ArgV0, P);
}

int main(int argc, char **argv, char * const *envp) {
  atexit(llvm_shutdown);

  cl::ParseCommandLineOptions(argc, argv, "seec clang test\n");

  llvm::InitializeNativeTarget();

  llvm::sys::Path ExecutablePath = GetExecutablePath(argv[0], true);

  // Setup diagnostics printing
  IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts
    = new clang::DiagnosticOptions();
  DiagOpts->ShowColors = true;

  TextDiagnosticPrinter DiagnosticPrinter(errs(), &*DiagOpts);

  llvm::IntrusiveRefCntPtr<DiagnosticsEngine> Diagnostics
    = new DiagnosticsEngine(
        IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()),
        &*DiagOpts,
        &DiagnosticPrinter,
        false);

  Diagnostics->setSuppressSystemWarnings(true);

  // Get a CompilerInvocation to parse+sema a single C99 source file
  auto Invocation = GetCompileForSourceFile(InputFile.c_str(),
                                            ExecutablePath.str(),
                                            Diagnostics);

  // Make Clang emit metadata with pointers to Decls
  Invocation->getCodeGenOpts().EmitDeclMetadata = 1;

  // Make an action to generate an LLVM Module (in memory only)
  OwningPtr<SeeCCodeGenAction> Action(new SeeCCodeGenAction());

  // Create a compiler instance to handle the actual work.
  CompilerInstance Compiler;
  Compiler.setInvocation(Invocation.release());
  Compiler.setDiagnostics(Diagnostics.getPtr());

  if (!Compiler.ExecuteAction(*Action))
    exit(EXIT_FAILURE);

  // Get the generated LLVM Module
  llvm::Module *Mod = Action->takeModule();
  if (!Mod) {
    errs() << "no Module generated\n";
    exit(EXIT_FAILURE);
  }

  GenerateSerializableMappings(*Action,
                               Mod,
                               Compiler.getSourceManager(),
                               InputFile);
  
  // Store all used source files into the LLVM Module.
  StoreCompileInformationInModule(Mod, Compiler);

  // Write the LLVM Module to a file
  std::string Error;

  raw_fd_ostream ModOut(ModuleOutputFile.c_str(), Error);
  Mod->print(ModOut, nullptr);
  ModOut.close();

  exit(EXIT_SUCCESS);
}
