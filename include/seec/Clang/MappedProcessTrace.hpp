//===- include/seec/Clang/MappedProcessTrace.hpp --------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines ProcessTrace, a class for interacting with SeeC-Clang
/// mapped process traces.
///
//===----------------------------------------------------------------------===//

#include "seec/Clang/MappedAST.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include <memory>


namespace seec {

/// Interfaces to SeeC-Clang mapped traces and states.
namespace cm {


class ProcessTrace {
  std::unique_ptr<seec::trace::InputBufferAllocator> BufferAllocator;
  
  std::unique_ptr<seec::trace::ProcessTrace> UnmappedTrace;
  
  std::shared_ptr<seec::ModuleIndex> ModuleIndex;
  
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts;
  
  clang::IgnoringDiagConsumer DiagConsumer;
  
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics;
  
  seec::seec_clang::MappedModule Mapping;
  
  ProcessTrace(llvm::StringRef ExecutablePath,
               std::unique_ptr<seec::trace::InputBufferAllocator> &&Allocator,
               std::unique_ptr<seec::trace::ProcessTrace> &&Trace,
               std::shared_ptr<seec::ModuleIndex> Index)
  : BufferAllocator(std::move(Allocator)),
    UnmappedTrace(std::move(Trace)),
    ModuleIndex(Index),
    DiagOpts(new clang::DiagnosticOptions()),
    DiagConsumer(),
    Diagnostics(new clang::DiagnosticsEngine(llvm::IntrusiveRefCntPtr
                                               <clang::DiagnosticIDs>
                                               (new clang::DiagnosticIDs()),
                                             &*DiagOpts,
                                             &DiagConsumer,
                                             false)),
    Mapping(*ModuleIndex, ExecutablePath, Diagnostics)
  {}
  
public:
  static
  seec::util::Maybe<std::unique_ptr<ProcessTrace>,
                    seec::Error>
  load(llvm::StringRef ExecutablePath,
       std::unique_ptr<seec::trace::InputBufferAllocator> &&Allocator);
};


} // namespace cm (in seec)

} // namespace seec
