//===- OpenTrace.hpp ------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "OpenTrace.hpp"

#include "llvm/Support/IRReader.h"

#include <wx/stdpaths.h>

seec::util::Maybe<std::unique_ptr<OpenTrace>,
                  char const *>
OpenTrace::FromFilePath(wxString const &FilePath) {
  typedef seec::util::Maybe<std::unique_ptr<OpenTrace>, char const *> RetTy;

  // Create an InputBufferAllocator for the folder containing the trace file.
  wxStandardPaths StdPaths;
  auto const &ExecutablePath = StdPaths.GetExecutablePath().ToStdString();

  llvm::sys::Path DirPath {FilePath.ToStdString()};
  DirPath.eraseComponent(); // Erase the filename from the path.

  auto BufferAllocator = seec::makeUnique<seec::trace::InputBufferAllocator>
                                         (DirPath);

  // Read the process trace using the InputBufferAllocator.
  auto MaybeProcTrace = seec::trace::ProcessTrace::readFrom(*BufferAllocator);
  if (!MaybeProcTrace.assigned(0)) {
    return RetTy("OpenTrace_Error_LoadProcessTrace");
  }

  auto ProcTrace = std::move(MaybeProcTrace.get<0>());
  assert(ProcTrace);

  // Load the bitcode.
  llvm::LLVMContext &Context = llvm::getGlobalContext();

  llvm::SMDiagnostic ParseError;
  llvm::Module *Mod = llvm::ParseIRFile(ProcTrace->getModuleIdentifier(),
                                        ParseError,
                                        Context);
  if (!Mod) {
    // ParseError.print("seec-trace-view", llvm::errs());
    return RetTy("OpenTrace_Error_ParseIRFile");
  }
    
  return RetTy(std::unique_ptr<OpenTrace>(
                  new OpenTrace(ExecutablePath,
                                Context,
                                std::move(BufferAllocator),
                                std::move(ProcTrace),
                                Mod)));
}
