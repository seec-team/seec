//===- tools/seec-cc/cc1_main.cpp -----------------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file This is primarily taken from Clang's tools/driver/cc1_main.cpp.
///
//===----------------------------------------------------------------------===//

#include "seec/Clang/Compile.hpp"
#include "seec/ICU/Output.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Driver/Arg.h"
#include "clang/Driver/ArgList.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/OptTable.h"
#include "clang/Driver/Option.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/FrontendTool/Utils.h"

#include "llvm/LinkAllPasses.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"

#include "unicode/locid.h"
#include "unicode/unistr.h"

#include <cstdio>
#include <string>

using namespace clang;

static void LLVMErrorHandler(void *UserData, const std::string &Message,
                             bool GenCrashDiag) {
  DiagnosticsEngine &Diags = *static_cast<DiagnosticsEngine*>(UserData);

  Diags.Report(diag::err_fe_error_backend) << Message;

  // Run the interrupt handlers to make sure any special cleanups get done, in
  // particular that we remove files registered with RemoveFileOnSignal.
  llvm::sys::RunInterruptHandlers();

  // We cannot recover from llvm errors.  When reporting a fatal error, exit
  // with status 70 to generate crash diagnostics.  For BSD systems this is
  // defined as an internal software error.  Otherwise, exit with status 1.
  exit(GenCrashDiag ? 70 : 1);
}

static FrontendAction *CreateFrontendBaseAction(CompilerInstance &CI,
                                                const char **ArgBegin,
                                                const char **ArgEnd)
{
  using namespace seec::seec_clang;
  using namespace clang::frontend;
  StringRef Action("unknown");

  switch (CI.getFrontendOpts().ProgramAction) {
  case ASTDeclList:            return new ASTDeclListAction();
  case ASTDump:                return new ASTDumpAction();
  case ASTDumpXML:             return new ASTDumpXMLAction();
  case ASTPrint:               return new ASTPrintAction();
  case ASTView:                return new ASTViewAction();
  case DumpRawTokens:          return new DumpRawTokensAction();
  case DumpTokens:             return new DumpTokensAction();
  case EmitAssembly:           return new SeeCEmitAssemblyAction(ArgBegin,
                                                                 ArgEnd);
  case EmitBC:                 return new SeeCEmitBCAction(ArgBegin, ArgEnd);
#ifdef CLANG_ENABLE_REWRITER
  case EmitHTML:               return new HTMLPrintAction();
#else
  case EmitHTML:               Action = "EmitHTML"; break;
#endif
  case EmitLLVM:               return new SeeCEmitLLVMAction(ArgBegin, ArgEnd);
  case EmitLLVMOnly:           return new SeeCEmitLLVMOnlyAction(ArgBegin,
                                                                 ArgEnd);
  case EmitCodeGenOnly:        return new SeeCEmitCodeGenOnlyAction(ArgBegin,
                                                                    ArgEnd);
  case EmitObj:                return new SeeCEmitBCAction(ArgBegin, ArgEnd);
#ifdef CLANG_ENABLE_REWRITER
  case FixIt:                  return new FixItAction();
#else
  case FixIt:                  Action = "FixIt"; break;
#endif
  case GenerateModule:         return new GenerateModuleAction;
  case GeneratePCH:            return new GeneratePCHAction;
  case GeneratePTH:            return new GeneratePTHAction();
  case InitOnly:               return new InitOnlyAction();
  case ParseSyntaxOnly:        return new SyntaxOnlyAction();
  case ModuleFileInfo:         return new DumpModuleInfoAction();

  case PluginAction: {
    for (FrontendPluginRegistry::iterator it =
           FrontendPluginRegistry::begin(), ie = FrontendPluginRegistry::end();
         it != ie; ++it) {
      if (it->getName() == CI.getFrontendOpts().ActionName) {
        OwningPtr<PluginASTAction> P(it->instantiate());
        if (!P->ParseArgs(CI, CI.getFrontendOpts().PluginArgs))
          return 0;
        return P.take();
      }
    }

    CI.getDiagnostics().Report(diag::err_fe_invalid_plugin_name)
      << CI.getFrontendOpts().ActionName;
    return 0;
  }

  case PrintDeclContext:       return new DeclContextPrintAction();
  case PrintPreamble:          return new PrintPreambleAction();
  case PrintPreprocessedInput: {
    if (CI.getPreprocessorOutputOpts().RewriteIncludes) {
#ifdef CLANG_ENABLE_REWRITER
      return new RewriteIncludesAction();
#else
      Action = "RewriteIncludesAction";
      break;
#endif
    }
    return new PrintPreprocessedAction();
  }

#ifdef CLANG_ENABLE_REWRITER
  case RewriteMacros:          return new RewriteMacrosAction();
  case RewriteObjC:            return new RewriteObjCAction();
  case RewriteTest:            return new RewriteTestAction();
#else
  case RewriteMacros:          Action = "RewriteMacros"; break;
  case RewriteObjC:            Action = "RewriteObjC"; break;
  case RewriteTest:            Action = "RewriteTest"; break;
#endif
#ifdef CLANG_ENABLE_ARCMT
  case MigrateSource:          return new arcmt::MigrateSourceAction();
#else
  case MigrateSource:          Action = "MigrateSource"; break;
#endif
#ifdef CLANG_ENABLE_STATIC_ANALYZER
  case RunAnalysis:            return new ento::AnalysisAction();
#else
  case RunAnalysis:            Action = "RunAnalysis"; break;
#endif
  case RunPreprocessorOnly:    return new PreprocessOnlyAction();
  }

#if !defined(CLANG_ENABLE_ARCMT) || !defined(CLANG_ENABLE_STATIC_ANALYZER) \
  || !defined(CLANG_ENABLE_REWRITER)
  CI.getDiagnostics().Report(diag::err_fe_action_not_available) << Action;
  return 0;
#else
  llvm_unreachable("Invalid program action!");
#endif
}

static FrontendAction *CreateFrontendAction(CompilerInstance &CI,
                                            const char **ArgBegin,
                                            const char **ArgEnd) {
  // Create the underlying action.
  FrontendAction *Act = CreateFrontendBaseAction(CI, ArgBegin, ArgEnd);
  if (!Act)
    return 0;

  const FrontendOptions &FEOpts = CI.getFrontendOpts();

#ifdef CLANG_ENABLE_REWRITER
  if (FEOpts.FixAndRecompile) {
    Act = new FixItRecompile(Act);
  }
#endif
  
#ifdef CLANG_ENABLE_ARCMT
  // Potentially wrap the base FE action in an ARC Migrate Tool action.
  switch (FEOpts.ARCMTAction) {
  case FrontendOptions::ARCMT_None:
    break;
  case FrontendOptions::ARCMT_Check:
    Act = new arcmt::CheckAction(Act);
    break;
  case FrontendOptions::ARCMT_Modify:
    Act = new arcmt::ModifyAction(Act);
    break;
  case FrontendOptions::ARCMT_Migrate:
    Act = new arcmt::MigrateAction(Act,
                                   FEOpts.MTMigrateDir,
                                   FEOpts.ARCMTMigrateReportOut,
                                   FEOpts.ARCMTMigrateEmitARCErrors);
    break;
  }

  if (FEOpts.ObjCMTAction != FrontendOptions::ObjCMT_None) {
    Act = new arcmt::ObjCMigrateAction(Act, FEOpts.MTMigrateDir,
                   FEOpts.ObjCMTAction & ~FrontendOptions::ObjCMT_Literals,
                   FEOpts.ObjCMTAction & ~FrontendOptions::ObjCMT_Subscripting);
  }
#endif

  // If there are any AST files to merge, create a frontend action
  // adaptor to perform the merge.
  if (!FEOpts.ASTMergeFiles.empty())
    Act = new ASTMergeAction(Act, FEOpts.ASTMergeFiles);

  return Act;
}

static bool DoCompilerInvocation(CompilerInstance *Clang,
                                 const char **ArgBegin,
                                 const char **ArgEnd)
{
  // Honor -help.
  if (Clang->getFrontendOpts().ShowHelp) {
    OwningPtr<driver::OptTable> Opts(driver::createDriverOptTable());
    Opts->PrintHelp(llvm::outs(), "clang -cc1",
                    "LLVM 'Clang' Compiler: http://clang.llvm.org",
                    /*Include=*/driver::options::CC1Option,
                    /*Exclude=*/0);
    return 0;
  }

  // Honor -version.
  //
  // FIXME: Use a better -version message?
  if (Clang->getFrontendOpts().ShowVersion) {
    llvm::cl::PrintVersionMessage();
    return 0;
  }

  // Load any requested plugins.
  for (unsigned i = 0,
         e = Clang->getFrontendOpts().Plugins.size(); i != e; ++i) {
    const std::string &Path = Clang->getFrontendOpts().Plugins[i];
    std::string Error;
    if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(Path.c_str(), &Error))
      Clang->getDiagnostics().Report(diag::err_fe_unable_to_load_plugin)
        << Path << Error;
  }

  // Honor -mllvm.
  //
  // FIXME: Remove this, one day.
  // This should happen AFTER plugins have been loaded!
  if (!Clang->getFrontendOpts().LLVMArgs.empty()) {
    unsigned NumArgs = Clang->getFrontendOpts().LLVMArgs.size();
    const char **Args = new const char*[NumArgs + 2];
    Args[0] = "clang (LLVM option parsing)";
    for (unsigned i = 0; i != NumArgs; ++i)
      Args[i + 1] = Clang->getFrontendOpts().LLVMArgs[i].c_str();
    Args[NumArgs + 1] = 0;
    llvm::cl::ParseCommandLineOptions(NumArgs + 1, Args);
  }

#ifdef CLANG_ENABLE_STATIC_ANALYZER
  // Honor -analyzer-checker-help.
  // This should happen AFTER plugins have been loaded!
  if (Clang->getAnalyzerOpts()->ShowCheckerHelp) {
    ento::printCheckerHelp(llvm::outs(), Clang->getFrontendOpts().Plugins);
    return 0;
  }
#endif

  // If there were errors in processing arguments, don't do anything else.
  if (Clang->getDiagnostics().hasErrorOccurred())
    return false;
  
  // Make Clang emit metadata with pointers to Decls.
  Clang->getInvocation().getCodeGenOpts().EmitDeclMetadata = 1;
  
  // Create and execute the frontend action.
  OwningPtr<FrontendAction> Act(CreateFrontendAction(*Clang, ArgBegin, ArgEnd));
  if (!Act)
    return false;
  
  bool Success = Clang->ExecuteAction(*Act);
  
  if (Clang->getFrontendOpts().DisableFree)
    Act.take();
  
  return Success;
}

int cc1_main(const char **ArgBegin,
             const char **ArgEnd,
             const char *Argv0,
             void *MainAddr)
{
  OwningPtr<CompilerInstance> Clang(new CompilerInstance());
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  // Initialize targets first, so that --version shows registered targets.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Buffer diagnostics from argument parsing so that we can output them using a
  // well formed diagnostic object.
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticBuffer *DiagsBuffer = new TextDiagnosticBuffer;
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);
  bool Success;
  Success = CompilerInvocation::CreateFromArgs(Clang->getInvocation(),
                                               ArgBegin, ArgEnd, Diags);

  // Infer the builtin include path if unspecified.
  if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang->getHeaderSearchOpts().ResourceDir.empty())
  {
    std::string ExecutablePath =
      llvm::sys::Path::GetMainExecutable(Argv0, MainAddr).str();
    
    Clang->getHeaderSearchOpts().ResourceDir =
      seec::seec_clang::getResourcesDirectory(ExecutablePath);
  }

  // Create the actual diagnostics engine.
  Clang->createDiagnostics();
  if (!Clang->hasDiagnostics())
    return 1;

  // Set an error handler, so that any LLVM backend diagnostics go through our
  // error handler.
  llvm::install_fatal_error_handler(LLVMErrorHandler,
                                  static_cast<void*>(&Clang->getDiagnostics()));

  DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());
  if (!Success)
    return 1;
  
  // Execute the frontend actions.
  Success = DoCompilerInvocation(Clang.get(), ArgBegin, ArgEnd);

  // If any timers were active but haven't been destroyed yet, print their
  // results now.  This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());

  // Our error handler depends on the Diagnostics object, which we're
  // potentially about to delete. Uninstall the handler now so that any
  // later errors use the default handling behavior instead.
  llvm::remove_fatal_error_handler();

  // When running with -disable-free, don't do any destruction or shutdown.
  if (Clang->getFrontendOpts().DisableFree) {
    if (llvm::AreStatisticsEnabled() || Clang->getFrontendOpts().ShowStats)
      llvm::PrintStatistics();
    Clang.take();
    return !Success;
  }

  // Managed static deconstruction. Useful for making things like
  // -time-passes usable.
  llvm::llvm_shutdown();

  return !Success;
}
