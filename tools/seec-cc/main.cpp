//===- tools/seec-cc/main.cpp ---------------------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file Most of this is taken directly from Clang's tools/driver/driver.cpp. 
///
//===----------------------------------------------------------------------===//

#include "seec/Clang/Compile.hpp"

#include "clang/Basic/CharInfo.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Action.h"
#include "clang/Driver/ArgList.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/OptTable.h"
#include "clang/Driver/Option.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"

using namespace clang;
using namespace clang::driver;

extern int cc1_main(const char **ArgBegin, const char **ArgEnd,
                    const char *Argv0, void *MainAddr);

// from clang's driver.cpp
llvm::sys::Path GetExecutablePath(const char *Argv0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return llvm::sys::Path(Argv0);

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void*) (intptr_t) GetExecutablePath;
  return llvm::sys::Path::GetMainExecutable(Argv0, P);
}

static const char *SaveStringInSet(std::set<std::string> &SavedStrings,
                                   StringRef S) {
  return SavedStrings.insert(S).first->c_str();
}

static void ExpandArgsFromBuf(const char *Arg,
                              SmallVectorImpl<const char*> &ArgVector,
                              std::set<std::string> &SavedStrings) {
  const char *FName = Arg + 1;
  OwningPtr<llvm::MemoryBuffer> MemBuf;
  if (llvm::MemoryBuffer::getFile(FName, MemBuf)) {
    ArgVector.push_back(SaveStringInSet(SavedStrings, Arg));
    return;
  }

  const char *Buf = MemBuf->getBufferStart();
  char InQuote = ' ';
  std::string CurArg;

  for (const char *P = Buf; ; ++P) {
    if (*P == '\0' || (isWhitespace(*P) && InQuote == ' ')) {
      if (!CurArg.empty()) {

        if (CurArg[0] != '@') {
          ArgVector.push_back(SaveStringInSet(SavedStrings, CurArg));
        } else {
          ExpandArgsFromBuf(CurArg.c_str(), ArgVector, SavedStrings);
        }

        CurArg = "";
      }
      if (*P == '\0')
        break;
      else
        continue;
    }

    if (isWhitespace(*P)) {
      if (InQuote != ' ')
        CurArg.push_back(*P);
      continue;
    }

    if (*P == '"' || *P == '\'') {
      if (InQuote == *P)
        InQuote = ' ';
      else if (InQuote == ' ')
        InQuote = *P;
      else
        CurArg.push_back(*P);
      continue;
    }

    if (*P == '\\') {
      ++P;
      if (*P != '\0')
        CurArg.push_back(*P);
      continue;
    }
    CurArg.push_back(*P);
  }
}

static void ExpandArgv(int argc, const char **argv,
                       SmallVectorImpl<const char*> &ArgVector,
                       std::set<std::string> &SavedStrings) {
  for (int i = 0; i < argc; ++i) {
    const char *Arg = argv[i];
    if (Arg[0] != '@') {
      // The following arguments are denied by SeeC:
      auto const Ref = llvm::StringRef(Arg);
      if (Ref.startswith("-g"))
        continue;

      ArgVector.push_back(SaveStringInSet(SavedStrings, std::string(Arg)));
      continue;
    }

    ExpandArgsFromBuf(Arg, ArgVector, SavedStrings);
  }
}

static void ParseProgName(SmallVectorImpl<const char *> &ArgVector,
                          std::set<std::string> &SavedStrings,
                          Driver &TheDriver)
{
  // Try to infer frontend type and default target from the program name.

  // suffixes[] contains the list of known driver suffixes.
  // Suffixes are compared against the program name in order.
  // If there is a match, the frontend type is updated as necessary (CPP/C++).
  // If there is no match, a second round is done after stripping the last
  // hyphen and everything following it. This allows using something like
  // "clang++-2.9".

  // If there is a match in either the first or second round,
  // the function tries to identify a target as prefix. E.g.
  // "x86_64-linux-clang" as interpreted as suffix "clang" with
  // target prefix "x86_64-linux". If such a target prefix is found,
  // is gets added via -target as implicit first argument.
  static const struct {
    const char *Suffix;
    bool IsCXX;
    bool IsCPP;
  } suffixes [] = {
    { "clang", false, false },
    { "clang++", true, false },
    { "clang-c++", true, false },
    { "clang-cc", false, false },
    { "clang-cpp", false, true },
    { "clang-g++", true, false },
    { "clang-gcc", false, false },
    { "seec-cc", false, false },
    { "cc", false, false },
    { "cpp", false, true },
    { "++", true, false },
  };
  std::string ProgName(llvm::sys::path::stem(ArgVector[0]));
  StringRef ProgNameRef(ProgName);
  StringRef Prefix;

  for (int Components = 2; Components; --Components) {
    bool FoundMatch = false;
    size_t i;

    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
      if (ProgNameRef.endswith(suffixes[i].Suffix)) {
        FoundMatch = true;
        if (suffixes[i].IsCXX)
          TheDriver.CCCIsCXX = true;
        if (suffixes[i].IsCPP)
          TheDriver.CCCIsCPP = true;
        break;
      }
    }

    if (FoundMatch) {
      StringRef::size_type LastComponent = ProgNameRef.rfind('-',
        ProgNameRef.size() - strlen(suffixes[i].Suffix));
      if (LastComponent != StringRef::npos)
        Prefix = ProgNameRef.slice(0, LastComponent);
      break;
    }

    StringRef::size_type LastComponent = ProgNameRef.rfind('-');
    if (LastComponent == StringRef::npos)
      break;
    ProgNameRef = ProgNameRef.slice(0, LastComponent);
  }

  if (Prefix.empty())
    return;

  std::string IgnoredError;
  if (llvm::TargetRegistry::lookupTarget(Prefix, IgnoredError)) {
    SmallVectorImpl<const char *>::iterator it = ArgVector.begin();
    if (it != ArgVector.end())
      ++it;
    ArgVector.insert(it, SaveStringInSet(SavedStrings, Prefix));
    ArgVector.insert(it,
      SaveStringInSet(SavedStrings, std::string("-target")));
  }
}

Command *MakeReplacementCommand(Command *C,
                                char const *InstalledDir,
                                std::set<std::string> &SavedStrings)
{
  switch (C->getSource().getKind()) {
    // If this is a linking command then replace it with seec-ld.
    case Action::LinkJobClass:
    {
      auto Args = C->getArguments();
      
      // Inform seec-ld of the real linker.
      Args.push_back("--seec");
      Args.push_back("-use-ld");
      Args.push_back(C->getExecutable());
      
      // Get the path to seec-ld.
      llvm::SmallString<256> LDPath (InstalledDir);
      llvm::sys::path::append(LDPath, "seec-ld");
      
      return new Command(C->getSource(),
                         C->getCreator(),
                         SaveStringInSet(SavedStrings, LDPath),
                         Args);
    }
    
    default:
      return nullptr;
  }
}

void ReplaceCommandsForSeeC(JobList &Jobs,
                            char const *InstalledDir,
                            std::set<std::string> &SavedStrings)
{
  for (auto It = Jobs.begin(), End = Jobs.end(); It != End; ++It) {
    if (auto C = llvm::dyn_cast<Command>(*It)) {
      auto Replacement = MakeReplacementCommand(C, InstalledDir, SavedStrings);
      if (!Replacement)
        continue;
      
      delete *It;
      
      *It = Replacement;
    }
    else if (auto JL = llvm::dyn_cast<JobList>(*It)) {
      ReplaceCommandsForSeeC(*JL, InstalledDir, SavedStrings);
    }
  }
}

void ReplaceCommandsForSeeC(Compilation &C,
                            char const *InstalledDir,
                            std::set<std::string> &SavedStrings)
{
  ReplaceCommandsForSeeC(C.getJobs(), InstalledDir, SavedStrings);
}

int main(int argc_, const char **argv_) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc_, argv_);

  std::set<std::string> SavedStrings;
  SmallVector<const char*, 256> argv;

  ExpandArgv(argc_, argv_, argv, SavedStrings);

  // Handle -cc1 integrated tools.
  if (argv.size() > 1 && StringRef(argv[1]).startswith("-cc1")) {
    StringRef Tool = argv[1] + 4;

    if (Tool == "")
      return cc1_main(argv.data()+2, argv.data()+argv.size(), argv[0],
                      (void*) (intptr_t) GetExecutablePath);

    // Reject unknown tools.
    llvm::errs() << "error: unknown integrated tool '" << Tool << "'\n";
    return 1;
  }

  bool CanonicalPrefixes = true;
  for (int i = 1, size = argv.size(); i < size; ++i) {
    if (StringRef(argv[i]) == "-no-canonical-prefixes") {
      CanonicalPrefixes = false;
      break;
    }
  }

  llvm::sys::Path Path = GetExecutablePath(argv[0], CanonicalPrefixes);
  
  // SeeC requires the following.
  {
    argv.push_back("-fno-builtin");
    argv.push_back("-D_FORTIFY_SOURCE=0");
    argv.push_back("-D__NO_CTYPE=1");
    
    using namespace seec::seec_clang;
    
    auto RTPath = SaveStringInSet(SavedStrings,
                                  getRuntimeLibraryDirectory(Path.str()));
    
    argv.push_back("-L");
    argv.push_back(RTPath);
    argv.push_back("-rpath");
    argv.push_back(RTPath);
    argv.push_back("-lseecRuntimeTracer");
    argv.push_back("-lpthread");
    argv.push_back("-ldl");
  }

  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions;
  {
    // Note that ParseDiagnosticArgs() uses the cc1 option table.
    OwningPtr<OptTable> CC1Opts(createDriverOptTable());
    unsigned MissingArgIndex, MissingArgCount;
    OwningPtr<InputArgList> Args(CC1Opts->ParseArgs(argv.begin()+1, argv.end(),
                                            MissingArgIndex, MissingArgCount));
    // We ignore MissingArgCount and the return value of ParseDiagnosticArgs.
    // Any errors that would be diagnosed here will also be diagnosed later,
    // when the DiagnosticsEngine actually exists.
    (void) ParseDiagnosticArgs(*DiagOpts, *Args);
  }
  // Now we can create the DiagnosticsEngine with a properly-filled-out
  // DiagnosticOptions instance.
  TextDiagnosticPrinter *DiagClient
    = new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);
  DiagClient->setPrefix(llvm::sys::path::filename(Path.str()));
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);
  ProcessWarningOptions(Diags, *DiagOpts, /*ReportDiags=*/false);

  Driver TheDriver(Path.str(), llvm::sys::getDefaultTargetTriple(),
                   "a.out", Diags);

  // Attempt to find the original path used to invoke the driver, to determine
  // the installed path. We do this manually, because we want to support that
  // path being a symlink.
  {
    SmallString<128> InstalledPath(argv[0]);

    // Do a PATH lookup, if there are no directory components.
    if (llvm::sys::path::filename(InstalledPath) == InstalledPath) {
      llvm::sys::Path Tmp = llvm::sys::Program::FindProgramByName(
        llvm::sys::path::filename(InstalledPath.str()));
      if (!Tmp.empty())
        InstalledPath = Tmp.str();
    }
    llvm::sys::fs::make_absolute(InstalledPath);
    InstalledPath = llvm::sys::path::parent_path(InstalledPath);
    bool exists;
    if (!llvm::sys::fs::exists(InstalledPath.str(), exists) && exists)
      TheDriver.setInstalledDir(InstalledPath);
  }
  
  TheDriver.ResourceDir = seec::seec_clang::getResourcesDirectory(Path.str());

  llvm::InitializeAllTargets();
  ParseProgName(argv, SavedStrings, TheDriver);

  // Handle CC_PRINT_OPTIONS and CC_PRINT_OPTIONS_FILE.
  TheDriver.CCPrintOptions = !!::getenv("CC_PRINT_OPTIONS");
  if (TheDriver.CCPrintOptions)
    TheDriver.CCPrintOptionsFilename = ::getenv("CC_PRINT_OPTIONS_FILE");

  // Handle CC_PRINT_HEADERS and CC_PRINT_HEADERS_FILE.
  TheDriver.CCPrintHeaders = !!::getenv("CC_PRINT_HEADERS");
  if (TheDriver.CCPrintHeaders)
    TheDriver.CCPrintHeadersFilename = ::getenv("CC_PRINT_HEADERS_FILE");

  // Handle CC_LOG_DIAGNOSTICS and CC_LOG_DIAGNOSTICS_FILE.
  TheDriver.CCLogDiagnostics = !!::getenv("CC_LOG_DIAGNOSTICS");
  if (TheDriver.CCLogDiagnostics)
    TheDriver.CCLogDiagnosticsFilename = ::getenv("CC_LOG_DIAGNOSTICS_FILE");

  OwningPtr<Compilation> C(TheDriver.BuildCompilation(argv));
  int Res = 0;
  SmallVector<std::pair<int, const Command *>, 4> FailingCommands;
  if (C.get()) {
    // Now we're going to intercept calls to the standard linker and replace
    // them with calls to seec-ld.
    ReplaceCommandsForSeeC(*C, TheDriver.getInstalledDir(), SavedStrings);
    Res = TheDriver.ExecuteCompilation(*C, FailingCommands);
  }

  for (SmallVectorImpl< std::pair<int, const Command *> >::iterator it =
         FailingCommands.begin(), ie = FailingCommands.end(); it != ie; ++it) {
    int CommandRes = it->first;
    const Command *FailingCommand = it->second;
    if (!Res)
      Res = CommandRes;

    // If result status is < 0, then the driver command signalled an error.
    // If result status is 70, then the driver command reported a fatal error.
    // In these cases, generate additional diagnostic information if possible.
    if (CommandRes < 0 || CommandRes == 70) {
      TheDriver.generateCompilationDiagnostics(*C, FailingCommand);
      break;
    }
  }

  // If any timers were active but haven't been destroyed yet, print their
  // results now.  This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());
  
  llvm::llvm_shutdown();

#ifdef _WIN32
  // Exit status should not be negative on Win32, unless abnormal termination.
  // Once abnormal termiation was caught, negative status should not be
  // propagated.
  if (Res < 0)
    Res = 1;
#endif

  // If we have multiple failing commands, we return the result of the first
  // failing command.
  return Res;
}
