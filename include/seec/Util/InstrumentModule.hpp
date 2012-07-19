//===- include/seec/Util/InstrumentModule.hpp ----------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_INSTRUMENT_MODULE_HPP_
#define _SEEC_UTIL_INSTRUMENT_MODULE_HPP_

#include "seec/Trace/ExecutionListener.hpp"
#include "seec/Transforms/RecordInternal/RecordInternal.hpp"
#include "seec/Transforms/RecordInternal/RecordPoints.hpp"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <memory>

using namespace llvm;

namespace seec {

/// Instruments a Module with an InternalRecordingListener and stores the
/// copy of the original Module, the ExecutionListener, and the
/// InternalRecordingListener. These will all be deleted when this class is
/// deconstructed.
/// \tparam LT the type of ExecutionListener being used. This class must have
///            a constructor taking (Module *OriginalModule, TargetData *TD).
template<class LT>
class InternalInstrumentation {
private:
  /// A copy of the original Module (pre-instrumentation).
  std::unique_ptr<Module> OriginalModule;

  /// The ExecutionListener being used.
  std::unique_ptr<LT> ExecutionListener;

  /// The InternalRecordingListener being used.
  std::unique_ptr<InternalRecordingListener> InternalListener;

public:
  /// Instruments a Module to support internal listening.
  /// \param M the Module to instrument.
  InternalInstrumentation(Module *M)
  : OriginalModule{CloneModule(M)},
    ExecutionListener{nullptr},
    InternalListener{nullptr}
  {
    std::string const &ModuleDataLayout = M->getDataLayout();

    PassManager Passes;
    Passes.add(new TargetLibraryInfo(Triple(M->getTargetTriple())));

    TargetData *TD = nullptr;
    if (!ModuleDataLayout.empty()) {
      TD = new TargetData(ModuleDataLayout);
      Passes.add(TD);
    }

    ExecutionListener.reset(new LT(OriginalModule.get(),
                                   new TargetData(ModuleDataLayout)));

    InternalListener.reset(new InternalRecordingListener(OriginalModule.get(),
                                                         TD,
                                                         ExecutionListener.get()
                                                         ));

    Passes.add(new InsertInternalRecording(InternalListener.get()));

    Passes.add(createVerifierPass());

    Passes.run(*M);
  }

  /// Get the original, uninstrumented Module.
  /// \return a pointer to the original, uninstrumented Module.
  Module const *originalModule() const { return OriginalModule.get(); }

  /// Get the ExecutionListener being used.
  /// \return a pointer to the ExecutionListener being used.
  LT *executionListener() { return ExecutionListener.get(); }

  /// Get the ExecutionListener being used.
  /// \return a pointer to the ExecutionListener being used.
  LT const *executionListener() const { return ExecutionListener.get(); }

  /// Get the InternalRecordingListener being used.
  /// \return a pointer to the InternalRecordingListener being used.
  InternalRecordingListener *internalListener() {
    return InternalListener.get();
  }

  /// Get the InternalRecordingListener being used.
  /// \return a pointer to the InternalRecordingListener being used.
  InternalRecordingListener const *internalListener() const {
    return InternalListener.get();
  }
};

} // namespace seec

#endif // _SEEC_UTIL_INSTRUMENT_MODULE_HPP_
