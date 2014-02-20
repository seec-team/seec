//===- tools/seec-ld/seec-ld.cpp ------------------------------------------===//
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
#include "seec/Util/MakeUnique.hpp"

#include "llvm/Linker.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"

#include <memory>

#include <unistd.h>


namespace {
  using namespace llvm;
  
  static cl::opt<std::string>
  LDPath("use-ld",
         cl::desc("linker"),
         cl::init("/usr/bin/ld"),
         cl::value_desc("filename"));
}

static void InitializeCodegen()
{
  using namespace llvm;
  
  // Initialize targets.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();
  
  // Initialize codegen and IR passes.
  PassRegistry *Registry = PassRegistry::getPassRegistry();
  initializeCore(*Registry);
  initializeCodeGen(*Registry);
  initializeLoopStrengthReducePass(*Registry);
  initializeLowerIntrinsicsPass(*Registry);
  initializeUnreachableBlockElimPass(*Registry);
}

static std::unique_ptr<llvm::Module> LoadFile(char const *ProgramName,
                                              std::string const &Filename,
                                              llvm::LLVMContext &Context)
{
  llvm::SMDiagnostic Err;
  auto Result = std::unique_ptr<llvm::Module>
                               (llvm::ParseIRFile(Filename, Err, Context));
  if (!Result)
    Err.print(ProgramName, llvm::errs());
  return Result;
}

/// \brief Add SeeC's instrumentation to the given Module.
/// \return true if the instrumentation was successful.
///
static bool Instrument(char const *ProgramName, llvm::Module &Module)
{
  llvm::PassManager Passes;

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  auto Triple = llvm::Triple(Module.getTargetTriple());
  if (Triple.getTriple().empty())
    Triple.setTriple(llvm::sys::getDefaultTargetTriple());
  Passes.add(new llvm::TargetLibraryInfo(Triple));

  // Add an appropriate DataLayout instance for this module.
  auto const &ModuleDataLayout = Module.getDataLayout();
  if (!ModuleDataLayout.empty())
    Passes.add(new llvm::DataLayout(ModuleDataLayout));

  // Add SeeC's recording instrumentation pass
  auto const Pass = new llvm::InsertExternalRecording();
  Passes.add(Pass);

  // Verify the final module
  Passes.add(llvm::createVerifierPass());

  // Run the passes
  Passes.run(Module);
  
  // Check if there were unhandled external functions.
  for (auto Fn : Pass->getUnhandledFunctions()) {
    llvm::errs() << ProgramName << ": function \""
                 << Fn->getName()
                 << "\" is not handled. If this function modifies memory state,"
                    " then SeeC will not be aware of it.\n";
  }
  
  return true;
}

static std::unique_ptr<llvm::tool_output_file>
GetTemporaryObjectStream(char const *ProgramName, llvm::SmallString<256> &Path)
{
  // TODO: If we ever support Win32 then extension should be ".obj".
  int FD;
  auto const Err =
    llvm::sys::fs::createUniqueFile("seec-instr-%%%%%%%%%%.o", FD, Path);
  
  if (Err != llvm::errc::success) {
    llvm::errs() << ProgramName << ": couldn't create temporary file.\n";
    exit(EXIT_FAILURE);
  }
  
  auto Out = seec::makeUnique<llvm::tool_output_file>(Path.c_str(), FD);
  if (!Out) {
    llvm::errs() << ProgramName << ": couldn't create temporary file.\n";
    exit(EXIT_FAILURE);
  }
  
  return Out;
}

static std::unique_ptr<llvm::tool_output_file>
Compile(char const *ProgramName,
        llvm::Module &Module,
        llvm::SmallString<256> &TempObjPath)
{
  auto Triple = llvm::Triple(Module.getTargetTriple());
  if (Triple.getTriple().empty())
    Triple.setTriple(llvm::sys::getDefaultTargetTriple());
  
  std::string ErrorMessage;
  auto const Target = llvm::TargetRegistry::lookupTarget(Triple.getTriple(),
                                                         ErrorMessage);
  if (!Target) {
    llvm::errs() << ProgramName << ": " << ErrorMessage << "\n";
    exit(EXIT_FAILURE);
  }
  
  // Target machine options.
  llvm::TargetOptions Options;
  
  std::unique_ptr<llvm::TargetMachine> Machine {
    Target->createTargetMachine(Triple.getTriple(),
                                /* cpu */ std::string{},
                                /* features */ std::string{},
                                Options,
                                llvm::Reloc::Default,
                                llvm::CodeModel::Default,
                                llvm::CodeGenOpt::Default)
  };
  
  assert(Machine && "Could not allocate target machine!");
  
  // Get an output file for the object.
  auto Out = GetTemporaryObjectStream(ProgramName, TempObjPath);
  
  // Setup all of the passes for the codegen.
  llvm::PassManager Passes;
  
  Passes.add(new llvm::TargetLibraryInfo(Triple));
  
  Machine->addAnalysisPasses(Passes);
  
  if (auto const *DL = Machine->getDataLayout())
    Passes.add(new llvm::DataLayout(*DL));
  else
    Passes.add(new llvm::DataLayout(&Module));
  
  llvm::formatted_raw_ostream FOS(Out->os());
  
  if (Machine->addPassesToEmitFile(Passes,
                                   FOS,
                                   llvm::TargetMachine::CGFT_ObjectFile)) {
    llvm::errs() << ProgramName << ": can't generate object file!\n";
    exit(EXIT_FAILURE);
  }
  
  Passes.run(Module);
  
  return Out;
}

static bool MaybeModule(char const *File)
{
  if (File[0] == '-')
    return false;
  
  llvm::sys::fs::file_status Status;
  if (llvm::sys::fs::status(File, Status) != llvm::errc::success)
    return false;
  
  if (!llvm::sys::fs::exists(Status))
    return false;
  
  // Don't attempt to read directories.
  if (llvm::sys::fs::is_directory(Status))
    return false;
  
  llvm::sys::fs::file_magic Magic;
  if (llvm::sys::fs::identify_magic(File, Magic) != llvm::errc::success)
    return false;
  
  switch (Magic) {
    // We will attempt to read files as assembly iff they end with ".ll".
    case llvm::sys::fs::file_magic::unknown:
      return llvm::StringRef(File).endswith(".ll");
    
    // Accept LLVM bitcode files.
    case llvm::sys::fs::file_magic::bitcode:
      return true;
    
    // Leave all other files for the real linker.
    default:
      return false;
  }
}

int main(int argc, char **argv)
{
  llvm::llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.
  auto &Context = llvm::getGlobalContext();
  
  // Setup the targets and passes required by codegen.
  InitializeCodegen();
  
  // Take all arguments that refer to LLVM bitcode files and link all of those
  // files together. The remaining arguments should be placed into ForwardArgs.
  std::vector<char const *> ForwardArgs;
  std::unique_ptr<llvm::Module> Composite;
  std::unique_ptr<llvm::Linker> Linker;
  std::size_t InsertCompositePathAt = 0;
  std::string ErrorMessage;
  
  ForwardArgs.push_back(argv[0]);
  
  for (auto i = 1; i < argc; ++i) {
    if (llvm::StringRef(argv[i]) == "--seec") {
      // Everything from here on in is a seec argument.
      argv[i] = argv[0];
      cl::ParseCommandLineOptions(argc - i, argv + i, "seec linker shim\n");
      break;
    }
    else if (MaybeModule(argv[i])) {
      // This argument is a file. Attempt to load it as an llvm::Module. If the
      // load fails then silently ignore it, as the file may be a native object
      // that we will pass to the real linker.
      auto Module = LoadFile(argv[0], argv[i], Context);
      if (Module) {
        if (Linker) {
          // Attempt to link this new Module to the existing Module.
          if (Linker->linkInModule(Module.get(), &ErrorMessage)) {
            llvm::errs() << argv[0] << ": error linking '" << argv[i] << "': "
                         << ErrorMessage << "\n";
            exit(EXIT_FAILURE);
          }
        }
        else {
          // This becomes our base Module.
          Composite = std::move(Module);
          Linker.reset(new llvm::Linker(Composite.get()));
          InsertCompositePathAt = ForwardArgs.size();
        }
        
        continue;
      }
    }
    
    // Whatever that argument was, it wasn't an llvm::Module, so we should
    // forward it to the real linker.
    ForwardArgs.push_back(argv[i]);
  }
  
  llvm::SmallString<256> TempObjPath;
  std::unique_ptr<llvm::tool_output_file> TempObj;
  
  if (Composite) {
    // Instrument the linked Module, if it exists.
    Instrument(argv[0], *Composite);
    
    // Codegen this Module to an object format and write it to a temporary file.
    TempObj = Compile(argv[0], *Composite, TempObjPath);
    
    // Insert the temporary file's path into the forwarding arguments.
    ForwardArgs.insert(ForwardArgs.begin() + InsertCompositePathAt,
                       TempObjPath.c_str());
  }
  else {
    llvm::errs() << argv[0] << ": didn't find any llvm modules.\n";
  }
  
  // Call the real ld with the unused original arguments and the new temporary
  // object file.
  ForwardArgs.push_back(nullptr);
  
  return llvm::sys::ExecuteAndWait(LDPath, ForwardArgs.data());
}
