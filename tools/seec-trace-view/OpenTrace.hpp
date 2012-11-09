//===- OpenTrace.hpp ------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_VIEW_OPENTRACE_HPP
#define SEEC_TRACE_VIEW_OPENTRACE_HPP

#include "seec/Clang/MappedAST.hpp"
#include "seec/Trace/TraceReader.hpp"
#include "seec/Util/MakeUnique.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/ModuleIndex.hpp"

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"

#include "llvm/LLVMContext.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/Path.h"

#include <wx/wx.h>
#include "seec/wxWidgets/CleanPreprocessor.h"

#include <memory>

/// \brief Holds information for a currently-open SeeC trace.
///
/// Holds all the information for a currently-open SeeC trace, including the
/// llvm::Module responsible for the trace, and the SeeC-Clang mapping to the
/// original source code.
class OpenTrace
{
  /// The context used by the Module.
  llvm::LLVMContext &Context;

  /// The InputBufferAllocator used to read this trace.
  std::unique_ptr<seec::trace::InputBufferAllocator> BufferAllocator;

  /// The ProcessTrace for this trace.
  std::shared_ptr<seec::trace::ProcessTrace> ProcTrace;

  /// The llvm::Module that this trace was produced from.
  llvm::Module *Module;

  /// An indexed view of Module.
  std::shared_ptr<seec::ModuleIndex> ModuleIndex;
  
  /// Diagnostics options used when reading original source code using Clang.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts;
  
  /// Diagnostics consumer used when reading original source code using Clang.
  clang::IgnoringDiagConsumer DiagConsumer;

  /// Diagnostics engine used when reading original source code using Clang.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diagnostics;

  /// Maps the Module back to the original source code.
  seec::seec_clang::MappedModule MapMod;

  /// Constructor.
  OpenTrace(llvm::StringRef ExecutablePath,
            llvm::LLVMContext &Context,
            std::unique_ptr<seec::trace::InputBufferAllocator> &&Allocator,
            std::shared_ptr<seec::trace::ProcessTrace> Trace,
            llvm::Module *Module)
  : Context(Context),
    BufferAllocator(std::move(Allocator)),
    ProcTrace(std::move(Trace)),
    Module(Module),
    ModuleIndex(new seec::ModuleIndex(*Module, true)),
    DiagOpts(new clang::DiagnosticOptions()),
    DiagConsumer(),
    Diagnostics(new clang::DiagnosticsEngine(
                      llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs>
                                              (new clang::DiagnosticIDs()),
                      &*DiagOpts,
                      &DiagConsumer,
                      false)),
    MapMod(*ModuleIndex, ExecutablePath, Diagnostics)
  {}

  // Don't allow copying.
  OpenTrace(OpenTrace const &) = delete;
  OpenTrace &operator=(OpenTrace const &) = delete;

public:
  /// Destructor.
  ~OpenTrace() = default;

  /// Attempt to read a trace at the given FilePath.
  /// \param FilePath the path to the process trace file.
  /// \return a seec::util::Maybe. If the trace was successfully read, then the
  ///         first element will be active and will contain a std::unique_ptr
  ///         holding an OpenTrace. If an error occurred, then the second
  ///         element will be active and will contain a pointer to a
  ///         statically-allocated C-String containing a key that can be used
  ///         to lookup the error in the GUIText table.
  static
  seec::util::Maybe<std::unique_ptr<OpenTrace>, wxString>
  FromFilePath(wxString const &FilePath);


  /// \name Accessors
  /// @{

  /// Get the context used by the Module.
  llvm::LLVMContext &getContext() const { return Context; }

  /// Get the ProcessTrace for this trace.
  seec::trace::ProcessTrace const &getProcessTrace() const {
    return *ProcTrace;
  }
  
  /// Get the shared_ptr for the ProcessTrace.
  decltype(ProcTrace) const &getProcessTracePtr() const {
    return ProcTrace;
  }

  /// Get the llvm::Module that this trace was produced from.
  llvm::Module const *getModule() const { return Module; };

  /// Get an indexed view of Module.
  seec::ModuleIndex const &getModuleIndex() const { return *ModuleIndex; }
  
  /// Get the shared_ptr for the indexed view of Module.
  decltype(ModuleIndex) const &getModuleIndexPtr() const { return ModuleIndex; }

  /// Get the MappedModule that maps the Module back to the original source
  /// code.
  seec::seec_clang::MappedModule const &getMappedModule() const {
    return MapMod;
  }

  /// @}
};

#endif // SEEC_TRACE_VIEW_OPENTRACE_HPP
