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

#ifndef SEEC_CLANG_MAPPEDPROCESSTRACE_HPP
#define SEEC_CLANG_MAPPEDPROCESSTRACE_HPP

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


/// \brief A SeeC-Clang-mapped process trace.
///
class ProcessTrace {
  /// The input buffer allocator for this process trace.
  std::unique_ptr<seec::trace::InputBufferAllocator> BufferAllocator;
  
  /// The base (unmapped) process trace.
  std::shared_ptr<seec::trace::ProcessTrace> UnmappedTrace;
  
  /// Indexed view of the llvm::Module.
  std::shared_ptr<seec::ModuleIndex> ModuleIndex;
  
  /// Diagnostic options for Clang.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts;
  
  /// Diagnostic consumer for Clang.
  clang::IgnoringDiagConsumer DiagConsumer;
  
  /// Diagnostics engine for Clang.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics;
  
  /// SeeC-Clang mapping.
  seec::seec_clang::MappedModule Mapping;
  
  /// \brief Constructor.
  ///
  ProcessTrace(llvm::StringRef ExecutablePath,
               std::unique_ptr<seec::trace::InputBufferAllocator> &&Allocator,
               std::shared_ptr<seec::trace::ProcessTrace> &&Trace,
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
  /// \brief Attempt to load a SeeC-Clang-mapped process trace.
  ///
  static
  seec::util::Maybe<std::unique_ptr<ProcessTrace>,
                    seec::Error>
  load(llvm::StringRef ExecutablePath,
       std::unique_ptr<seec::trace::InputBufferAllocator> &&Allocator);
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the base (unmapped) process trace.
  ///
  std::shared_ptr<seec::trace::ProcessTrace> getUnmappedTrace() const {
    return UnmappedTrace;
  };
  
  /// \brief Get the indexed view of the llvm::Module.
  ///
  std::shared_ptr<seec::ModuleIndex> getModuleIndex() const {
    return ModuleIndex;
  }
  
  /// @} (Access underlying information)
};


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDPROCESSTRACE_HPP
