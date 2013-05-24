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

#include "seec/ICU/LazyMessage.hpp"
#include "seec/Util/MakeUnique.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include "OpenTrace.hpp"

seec::Maybe<std::unique_ptr<OpenTrace>, seec::Error>
OpenTrace::FromFilePath(wxString const &FilePath)
{
  // Create an InputBufferAllocator for the folder containing the trace file.
  wxStandardPaths StdPaths;
  auto const &ExecutablePath = StdPaths.GetExecutablePath().ToStdString();

  llvm::error_code ErrCode;
  llvm::SmallString<256> DirPath {FilePath.ToStdString()};
  
  bool IsDirectory;
  ErrCode = llvm::sys::fs::is_directory(llvm::StringRef(DirPath), IsDirectory);
  
  if (ErrCode != llvm::errc::success) {
    return
      seec::Error(
        seec::LazyMessageByRef::create("TraceViewer",
                                       {"GUIText",
                                        "OpenTrace_Error_FailIsDirectory"}));
  }
  
  // If the FilePath is indeed a file, remove the filename.
  if (!IsDirectory)
    llvm::sys::path::remove_filename(DirPath);
  
  // Attempt to create an input allocator for the folder.
  auto MaybeIBA = seec::trace::InputBufferAllocator::createFor(DirPath);
  if (MaybeIBA.assigned<seec::Error>())
    return MaybeIBA.move<seec::Error>();
  
  assert(MaybeIBA.assigned<seec::trace::InputBufferAllocator>());
  
  // Attempt to load the SeeC-Clang Mapped process trace.
  auto IBAPtrTemp =
    seec::makeUnique<seec::trace::InputBufferAllocator>
                    (MaybeIBA.move<seec::trace::InputBufferAllocator>());
  
  auto MaybeTrace = seec::cm::ProcessTrace::load(ExecutablePath,
                                                 std::move(IBAPtrTemp));
  
  if (MaybeTrace.assigned<seec::Error>())
    return MaybeTrace.move<seec::Error>();
  
  auto TracePtr = MaybeTrace.move<std::unique_ptr<seec::cm::ProcessTrace>>();
  return std::unique_ptr<OpenTrace>(new OpenTrace(std::move(TracePtr)));
}
