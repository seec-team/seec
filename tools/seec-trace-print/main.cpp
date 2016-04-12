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
#include "seec/wxWidgets/AugmentResources.hpp"
#include "seec/wxWidgets/Config.hpp"

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

#include "unicode/unistr.h"

#include "ClangMapped.hpp"
#include "OnlinePythonTutor.hpp"
#include "Unmapped.hpp"

#include <array>
#include <memory>
#include <system_error>
#include <type_traits>

using namespace seec;
using namespace llvm;

namespace seec {
  namespace trace_print {
    cl::opt<std::string>
    InputDirectory(cl::desc("<input trace>"), cl::Positional, cl::init(""));

    cl::opt<bool>
    UseClangMapping("C", cl::desc("use SeeC-Clang mapped states"));

    cl::opt<std::string>
    OutputDirectoryForClangMappedDot("G", cl::desc("output dot graphs to this directory"));

    cl::opt<bool>
    TestGraphGeneration("graph-test", cl::desc("generate dot graphs (but do not write them)"));

    cl::opt<bool>
    ShowCounts("counts", cl::desc("show event counts"));

    cl::opt<bool>
    ShowRawEvents("R", cl::desc("show raw events"));

    cl::opt<bool>
    ShowStates("S", cl::desc("show recreated states"));

    cl::opt<bool>
    ShowErrors("E", cl::desc("show run-time errors"));

    cl::opt<bool>
    OnlinePythonTutor("P", cl::desc("output suitable for Online Python Tutor"));

    cl::opt<std::string>
    OPTVariableName("opt-var-name", cl::desc("for Online Python Tutor trace output, create a variable with this name"));

    cl::opt<bool>
    ReverseStates("reverse", cl::desc("show reverse iterated states at the end"));

    cl::opt<bool>
    ShowComparable("comparable", cl::desc("print comparable states (don't show raw addresses)"));

    cl::opt<bool>
    Quiet("quiet", cl::desc("don't print recreated states (for timing)"));

    cl::opt<bool>
    TestMovement("test-movement", cl::desc("test movement only"));
  }
}

using namespace seec::trace_print;

// From clang's driver.cpp:
std::string GetExecutablePath(const char *Argv0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return Argv0;

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void*) (intptr_t) GetExecutablePath;
  return llvm::sys::fs::getMainExecutable(Argv0, P);
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

  // Setup a dummy wxApp to enable some wxWidgets functionality.
  seec::setupDummyAppConsole();

  // Attempt to get common config files.
  if (!seec::setupCommonConfig()) {
    llvm::errs() << "Failed to setup configuration.\n";
  }

  // Load augmentations.
  seec::AugmentationCollection Augmentations;
  Augmentations.loadFromResources(ResourcePath);
  Augmentations.loadFromUserLocalDataDir();

  if (UseClangMapping || OnlinePythonTutor) {
    PrintClangMapped(Augmentations, OPTVariableName);
  }
  else {
    PrintUnmapped(Augmentations);
  }
  
  return EXIT_SUCCESS;
}
