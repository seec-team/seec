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
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedStateCommon.hpp"
#include "seec/ICU/LazyMessage.hpp"
#include "seec/Util/Error.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include <memory>


namespace llvm {
  class LLVMContext;
}


namespace seec {
  
namespace trace {
  class ProcessTrace;
  class InputBufferAllocator;
}

/// Interfaces to SeeC-Clang mapped traces and states.
namespace cm {


/// \brief A SeeC-Clang-mapped process trace.
///
class ProcessTrace {
  /// The \c LLVMContext for this trace's Module.
  std::unique_ptr<llvm::LLVMContext> TheContext;
  
  /// The \c Module for this process trace.
  std::unique_ptr<llvm::Module> TheModule;

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
  ProcessTrace(std::unique_ptr<llvm::LLVMContext> WithContext,
               std::unique_ptr<llvm::Module> WithModule,
               std::shared_ptr<seec::trace::ProcessTrace> Trace,
               std::shared_ptr<seec::ModuleIndex> Index)
  : TheContext(std::move(WithContext)),
    TheModule(std::move(WithModule)),
    UnmappedTrace(std::move(Trace)),
    ModuleIndex(std::move(Index)),
    DiagOpts(new clang::DiagnosticOptions()),
    DiagConsumer(),
    Diagnostics(new clang::DiagnosticsEngine(llvm::IntrusiveRefCntPtr
                                               <clang::DiagnosticIDs>
                                               (new clang::DiagnosticIDs()),
                                             &*DiagOpts,
                                             &DiagConsumer,
                                             false)),
    Mapping(*ModuleIndex, Diagnostics)
  {}
  
public:
  /// \brief Attempt to load a SeeC-Clang-mapped process trace.
  ///
  static
  seec::Maybe<std::unique_ptr<ProcessTrace>, seec::Error>
  load(std::unique_ptr<seec::trace::InputBufferAllocator> Allocator);
  
  
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
  
  /// \brief Get the SeeC-Clang mapping information.
  ///
  seec::seec_clang::MappedModule const &getMapping() const {
    return Mapping;
  }
  
  /// @} (Access underlying information)
  
  
  /// \brief Get the mapping information for the function at the given run-time
  ///        address.
  /// \param Address the run-time address.
  /// \return a pointer to the mapping information, or nullptr if no function
  ///         is known at the given address or no mapping information is
  ///         available for the function at the given address.
  ///
  seec::seec_clang::MappedFunctionDecl const *
  getMappedFunctionAt(stateptr_ty const Address) const;
};


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDPROCESSTRACE_HPP
