//===- tools/seec-trace-view/OpenTrace.cpp --------------------------------===//
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

  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(DirPath.str());
  if (MaybeIBA.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Message = MaybeIBA.get<seec::Error>().getMessage(Status, Locale());
    assert(U_SUCCESS(Status));
    return RetTy(seec::towxString(Message));
  }
  
  assert(MaybeIBA.assigned<seec::trace::InputBufferAllocator>());
  auto BufferAllocator = seec::makeUnique<seec::trace::InputBufferAllocator>
                                         (MaybeIBA.get<seec::trace::InputBufferAllocator>());

  // Read the process trace using the InputBufferAllocator.
  auto MaybeProcTrace = seec::trace::ProcessTrace::readFrom(*BufferAllocator);
  if (!MaybeProcTrace.assigned(0)) {
    // TODO: Display the message from the error in MaybeProcTrace.get<1>().
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
  
  auto MaybeMod = BufferAllocator->getModule(Context);
  if (MaybeMod.assigned<seec::Error>()) {
    UErrorCode Status = U_ZERO_ERROR;
    auto Message = MaybeMod.get<seec::Error>().getMessage(Status, Locale());
    assert(U_SUCCESS(Status));
    return RetTy(seec::towxString(Message));
  }
  
  assert(MaybeMod.assigned<llvm::Module *>());
  auto Mod = MaybeMod.get<llvm::Module *>();

  // Construct and return the OpenTrace object.
  return RetTy(std::unique_ptr<OpenTrace>(
                  new OpenTrace(ExecutablePath,
                                Context,
                                std::move(BufferAllocator),
                                std::move(ProcTrace),
                                Mod)));
}
