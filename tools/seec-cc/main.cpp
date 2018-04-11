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
#include "clang/Driver/Action.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/ToolChain.h"
#include "clang/Frontend/ChainedDiagnosticConsumer.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/SerializedDiagnosticPrinter.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <set>
#include <system_error>
using namespace clang;
using namespace clang::driver;
using namespace llvm::opt;

std::string GetExecutablePath(const char *Argv0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes) {
    SmallString<128> ExecutablePath(Argv0);
    // Do a PATH lookup if Argv0 isn't a valid path.
    if (!llvm::sys::fs::exists(ExecutablePath))
      if (llvm::ErrorOr<std::string> P =
              llvm::sys::findProgramByName(ExecutablePath))
        ExecutablePath = *P;
    return ExecutablePath.str();
  }

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void*) (intptr_t) GetExecutablePath;
  return llvm::sys::fs::getMainExecutable(Argv0, P);
}

static const char *GetStableCStr(std::set<std::string> &SavedStrings,
                                 StringRef S) {
  return SavedStrings.insert(S).first->c_str();
}

extern int cc1_main(ArrayRef<const char *> Argv, const char *Argv0,
                    void *MainAddr);

static void insertTargetAndModeArgs(const ParsedClangName &NameParts,
                                    SmallVectorImpl<const char *> &ArgVector,
                                    std::set<std::string> &SavedStrings) {
  // Put target and mode arguments at the start of argument list so that
  // arguments specified in command line could override them. Avoid putting
  // them at index 0, as an option like '-cc1' must remain the first.
  auto InsertionPoint = ArgVector.begin();
  if (InsertionPoint != ArgVector.end())
    ++InsertionPoint;

  if (NameParts.DriverMode) {
    // Add the mode flag to the arguments.
    ArgVector.insert(InsertionPoint,
                     GetStableCStr(SavedStrings, NameParts.DriverMode));
  }

  if (NameParts.TargetIsValid) {
    const char *arr[] = {"-target", GetStableCStr(SavedStrings,
                                                  NameParts.TargetPrefix)};
    ArgVector.insert(InsertionPoint, std::begin(arr), std::end(arr));
  }
}

Command *MakeReplacementCommand(Command *C,
                                ToolChain const &TC,
                                char const *InstalledDir,
                                std::set<std::string> &SavedStrings)
{
  switch (C->getSource().getKind()) {
    // If this is a linking command then replace it with seec-ld.
    case Action::LinkJobClass:
    {
      auto const &TCTriple = TC.getTriple();
      auto Args = C->getArguments();
      
      // Get the path to seec-ld.
      llvm::SmallString<256> LDPath (InstalledDir);
#if defined(_WIN32)
      llvm::sys::path::append(LDPath, "seec-ld.exe");
#else
      llvm::sys::path::append(LDPath, "seec-ld");
#endif
      
      // SeeC requires that we link additional libraries, including the runtime
      // library containing the tracing/error detection implementation.
      using namespace seec::seec_clang;
      
      auto RTPath = GetStableCStr(SavedStrings,
                                  getRuntimeLibraryDirectory(LDPath));
      
      Args.push_back("-L");
      Args.push_back(RTPath);

      if (!TCTriple.isOSWindows()) {
        Args.push_back("-rpath");
        Args.push_back(RTPath);
      }

      // TODO: this should perhaps depend on the target.
      Args.push_back("-lseecRuntimeTracer");

      if (!TCTriple.isOSWindows()) {
        Args.push_back("-lpthread");
        Args.push_back("-ldl");
      }
      
      // Inform seec-ld of the real linker.
      Args.push_back("--seec");
      Args.push_back("-use-ld");
      Args.push_back(C->getExecutable());
      
      return new Command(C->getSource(),
                         C->getCreator(),
                         GetStableCStr(SavedStrings, LDPath),
                         Args,
                         C->getInputFilenames());
    }
    
    default:
      return nullptr;
  }
}

void ReplaceCommandsForSeeC(JobList &Jobs,
                            ToolChain const &TC,
                            char const *InstalledDir,
                            std::set<std::string> &SavedStrings)
{
  for (auto &JobPtr : Jobs.getJobsMutable()) {
    if (auto R = MakeReplacementCommand(JobPtr.get(),
                                        TC,
                                        InstalledDir,
                                        SavedStrings))
    {
      JobPtr.reset(R);
    }
  }
}

void ReplaceCommandsForSeeC(Compilation &C,
                            char const *InstalledDir,
                            std::set<std::string> &SavedStrings)
{
  ReplaceCommandsForSeeC(C.getJobs(),
                         C.getDefaultToolChain(),
                         InstalledDir,
                         SavedStrings);
}

static void getCLEnvVarOptions(std::string &EnvValue, llvm::StringSaver &Saver,
                               SmallVectorImpl<const char *> &Opts) {
  llvm::cl::TokenizeWindowsCommandLine(EnvValue, Saver, Opts);
  // The first instance of '#' should be replaced with '=' in each option.
  for (const char *Opt : Opts)
    if (char *NumberSignPtr = const_cast<char *>(::strchr(Opt, '#')))
      *NumberSignPtr = '=';
}

static void SetBackdoorDriverOutputsFromEnvVars(Driver &TheDriver) {
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
}

static void FixupDiagPrefixExeName(TextDiagnosticPrinter *DiagClient,
                                   const std::string &Path) {
  // If the clang binary happens to be named cl.exe for compatibility reasons,
  // use clang-cl.exe as the prefix to avoid confusion between clang and MSVC.
  StringRef ExeBasename(llvm::sys::path::filename(Path));
  if (ExeBasename.equals_lower("cl.exe"))
    ExeBasename = "clang-cl.exe";
  DiagClient->setPrefix(ExeBasename);
}

// This lets us create the DiagnosticsEngine with a properly-filled-out
// DiagnosticOptions instance.
static DiagnosticOptions *
CreateAndPopulateDiagOpts(ArrayRef<const char *> argv) {
  auto *DiagOpts = new DiagnosticOptions;
  std::unique_ptr<OptTable> Opts(createDriverOptTable());
  unsigned MissingArgIndex, MissingArgCount;
  InputArgList Args =
      Opts->ParseArgs(argv.slice(1), MissingArgIndex, MissingArgCount);
  // We ignore MissingArgCount and the return value of ParseDiagnosticArgs.
  // Any errors that would be diagnosed here will also be diagnosed later,
  // when the DiagnosticsEngine actually exists.
  (void)ParseDiagnosticArgs(*DiagOpts, Args);
  return DiagOpts;
}

static void SetInstallDir(SmallVectorImpl<const char *> &argv,
                          Driver &TheDriver, bool CanonicalPrefixes) {
  // Attempt to find the original path used to invoke the driver, to determine
  // the installed path. We do this manually, because we want to support that
  // path being a symlink.
  SmallString<128> InstalledPath(argv[0]);

  // Do a PATH lookup, if there are no directory components.
  if (llvm::sys::path::filename(InstalledPath) == InstalledPath)
    if (llvm::ErrorOr<std::string> Tmp = llvm::sys::findProgramByName(
            llvm::sys::path::filename(InstalledPath.str())))
      InstalledPath = *Tmp;

  // FIXME: We don't actually canonicalize this, we just make it absolute.
  if (CanonicalPrefixes)
    llvm::sys::fs::make_absolute(InstalledPath);

  StringRef InstalledPathParent(llvm::sys::path::parent_path(InstalledPath));
  if (llvm::sys::fs::exists(InstalledPathParent))
    TheDriver.setInstalledDir(InstalledPathParent);
}

static int ExecuteCC1Tool(ArrayRef<const char *> argv, StringRef Tool) {
  void *GetExecutablePathVP = (void *)(intptr_t) GetExecutablePath;
  if (Tool == "")
    return cc1_main(argv.slice(2), argv[0], GetExecutablePathVP);

  // Reject unknown tools.
  llvm::errs() << "error: unknown integrated tool '" << Tool << "'\n";
  return 1;
}

int main(int argc_, const char **argv_) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv_[0]);
  llvm::PrettyStackTraceProgram X(argc_, argv_);
  llvm::llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  if (llvm::sys::Process::FixupStandardFileDescriptors())
    return 1;

  SmallVector<const char *, 256> argv;
  llvm::SpecificBumpPtrAllocator<char> ArgAllocator;
  std::error_code EC = llvm::sys::Process::GetArgumentVector(
      argv, llvm::makeArrayRef(argv_, argc_), ArgAllocator);
  if (EC) {
    llvm::errs() << "error: couldn't get arguments: " << EC.message() << '\n';
    return 1;
  }

  llvm::InitializeAllTargets();
  // for now - we should replace "seec" with "clang".
  std::string ProgName = "clang"; // argv[0];
  auto TargetAndMode = ToolChain::getTargetAndModeFromProgramName(argv[0]);

  llvm::BumpPtrAllocator A;
  llvm::StringSaver Saver(A);

  // Parse response files using the GNU syntax, unless we're in CL mode. There
  // are two ways to put clang in CL compatibility mode: argv[0] is either
  // clang-cl or cl, or --driver-mode=cl is on the command line. The normal
  // command line parsing can't happen until after response file parsing, so we
  // have to manually search for a --driver-mode=cl argument the hard way.
  // Finally, our -cc1 tools don't care which tokenization mode we use because
  // response files written by clang will tokenize the same way in either mode.
  bool ClangCLMode = false;
  if (StringRef(TargetAndMode.DriverMode).equals("--driver-mode=cl") ||
      std::find_if(argv.begin(), argv.end(), [](const char *F) {
        return F && strcmp(F, "--driver-mode=cl") == 0;
      }) != argv.end()) {
    ClangCLMode = true;
  }
  enum { Default, POSIX, Windows } RSPQuoting = Default;
  for (const char *F : argv) {
    if (strcmp(F, "--rsp-quoting=posix") == 0)
      RSPQuoting = POSIX;
    else if (strcmp(F, "--rsp-quoting=windows") == 0)
      RSPQuoting = Windows;
  }

  // Determines whether we want nullptr markers in argv to indicate response
  // files end-of-lines. We only use this for the /LINK driver argument with
  // clang-cl.exe on Windows.
  bool MarkEOLs = ClangCLMode;

  llvm::cl::TokenizerCallback Tokenizer;
  if (RSPQuoting == Windows || (RSPQuoting == Default && ClangCLMode))
    Tokenizer = &llvm::cl::TokenizeWindowsCommandLine;
  else
    Tokenizer = &llvm::cl::TokenizeGNUCommandLine;

  if (MarkEOLs && argv.size() > 1 && StringRef(argv[1]).startswith("-cc1"))
    MarkEOLs = false;
  llvm::cl::ExpandResponseFiles(Saver, Tokenizer, argv, MarkEOLs);

  // Handle -cc1 integrated tools, even if -cc1 was expanded from a response
  // file.
  auto FirstArg = std::find_if(argv.begin() + 1, argv.end(),
                               [](const char *A) { return A != nullptr; });
  if (FirstArg != argv.end() && StringRef(*FirstArg).startswith("-cc1")) {
    // If -cc1 came from a response file, remove the EOL sentinels.
    if (MarkEOLs) {
      auto newEnd = std::remove(argv.begin(), argv.end(), nullptr);
      argv.resize(newEnd - argv.begin());
    }
    return ExecuteCC1Tool(argv, argv[1] + 4);
  }

  bool CanonicalPrefixes = true;
  for (int i = 1, size = argv.size(); i < size; ++i) {
    // Skip end-of-line response file markers
    if (argv[i] == nullptr)
      continue;
    if (StringRef(argv[i]) == "-no-canonical-prefixes") {
      CanonicalPrefixes = false;
      break;
    }
  }

  // Handle CL and _CL_ which permits additional command line options to be
  // prepended or appended.
  if (ClangCLMode) {
    // Arguments in "CL" are prepended.
    llvm::Optional<std::string> OptCL = llvm::sys::Process::GetEnv("CL");
    if (OptCL.hasValue()) {
      SmallVector<const char *, 8> PrependedOpts;
      getCLEnvVarOptions(OptCL.getValue(), Saver, PrependedOpts);

      // Insert right after the program name to prepend to the argument list.
      argv.insert(argv.begin() + 1, PrependedOpts.begin(), PrependedOpts.end());
    }
    // Arguments in "_CL_" are appended.
    llvm::Optional<std::string> Opt_CL_ = llvm::sys::Process::GetEnv("_CL_");
    if (Opt_CL_.hasValue()) {
      SmallVector<const char *, 8> AppendedOpts;
      getCLEnvVarOptions(Opt_CL_.getValue(), Saver, AppendedOpts);

      // Insert at the end of the argument list to append.
      argv.append(AppendedOpts.begin(), AppendedOpts.end());
    }
  }

  std::set<std::string> SavedStrings;

  // SeeC requires the following.
  {
    argv.push_back("-fno-builtin");
    argv.push_back("-D_FORTIFY_SOURCE=0");
    argv.push_back("-D__NO_CTYPE=1");
    argv.push_back("-D__SEEC__");
  }

  std::string Path = GetExecutablePath(argv[0], CanonicalPrefixes);

  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts =
      CreateAndPopulateDiagOpts(argv);

  TextDiagnosticPrinter *DiagClient
    = new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);
  FixupDiagPrefixExeName(DiagClient, Path);

  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);

  if (!DiagOpts->DiagnosticSerializationFile.empty()) {
    auto SerializedConsumer =
        clang::serialized_diags::create(DiagOpts->DiagnosticSerializationFile,
                                        &*DiagOpts, /*MergeChildRecords=*/true);
    Diags.setClient(new ChainedDiagnosticConsumer(
        Diags.takeClient(), std::move(SerializedConsumer)));
  }

  ProcessWarningOptions(Diags, *DiagOpts, /*ReportDiags=*/false);

  Driver TheDriver(Path, llvm::sys::getDefaultTargetTriple(), Diags);
  SetInstallDir(argv, TheDriver, CanonicalPrefixes);
  TheDriver.setTargetAndMode(TargetAndMode);
  TheDriver.ResourceDir = seec::seec_clang::getResourcesDirectory(Path);

  insertTargetAndModeArgs(TargetAndMode, argv, SavedStrings);

  SetBackdoorDriverOutputsFromEnvVars(TheDriver);

  std::unique_ptr<Compilation> C(TheDriver.BuildCompilation(argv));
  int Res = 1;
  if (C && !C->containsError()) {
    SmallVector<std::pair<int, const Command *>, 4> FailingCommands;
    // Now we're going to intercept calls to the standard linker and replace
    // them with calls to seec-ld.
    ReplaceCommandsForSeeC(*C, TheDriver.getInstalledDir(), SavedStrings);
    Res = TheDriver.ExecuteCompilation(*C, FailingCommands);

    for (const auto &P : FailingCommands) {
      int CommandRes = P.first;
      const Command *FailingCommand = P.second;
      if (!Res)
        Res = CommandRes;

      // If result status is < 0, then the driver command signalled an error.
      // If result status is 70, then the driver command reported a fatal error.
      // On Windows, abort will return an exit code of 3.  In these cases,
      // generate additional diagnostic information if possible.
      bool DiagnoseCrash = CommandRes < 0 || CommandRes == 70;
#ifdef LLVM_ON_WIN32
      DiagnoseCrash |= CommandRes == 3;
#endif
      if (DiagnoseCrash) {
        TheDriver.generateCompilationDiagnostics(*C, *FailingCommand);
        break;
      }
    }
  }

  Diags.getClient()->finish();

  // If any timers were active but haven't been destroyed yet, print their
  // results now.  This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());

#ifdef LLVM_ON_WIN32
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
