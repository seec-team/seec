//===- OpenTrace.hpp ------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/wxWidgets/StringConversion.hpp"

#include "llvm/Support/IRReader.h"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "OpenTrace.hpp"

seec::util::Maybe<std::unique_ptr<OpenTrace>, wxString>
OpenTrace::FromFilePath(wxString const &FilePath) {
  typedef seec::util::Maybe<std::unique_ptr<OpenTrace>, wxString> RetTy;

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
    UErrorCode Status = U_ZERO_ERROR;
    auto TextTable = seec::getResource("TraceViewer",
                                       Locale::getDefault(),
                                       Status,
                                       "GUIText");
    assert(U_SUCCESS(Status));

    return RetTy(seec::getwxStringExOrDie(TextTable,
                                          "OpenTrace_Error_LoadProcessTrace"));
  }

  auto ProcTrace = std::move(MaybeProcTrace.get<0>());
  assert(ProcTrace);

  // Load the bitcode.
  llvm::LLVMContext &Context = llvm::getGlobalContext();

  DirPath.appendComponent(ProcTrace->getModuleIdentifier());

  llvm::SMDiagnostic ParseError;
  llvm::Module *Mod = llvm::ParseIRFile(DirPath.str(),
                                        ParseError,
                                        Context);
  if (!Mod) {
    // TODO: Add the parse error to the returned error message.
    //
    // ParseError.print("seec-trace-view", llvm::errs());

    UErrorCode Status = U_ZERO_ERROR;
    auto TextTable = seec::getResource("TraceViewer",
                                       Locale::getDefault(),
                                       Status,
                                       "GUIText");
    assert(U_SUCCESS(Status));

    return RetTy(seec::getwxStringExOrDie(TextTable,
                                          "OpenTrace_Error_ParseIRFile"));
  }

  return RetTy(std::unique_ptr<OpenTrace>(
                  new OpenTrace(ExecutablePath,
                                Context,
                                std::move(BufferAllocator),
                                std::move(ProcTrace),
                                Mod)));
}
