//===- lib/Runtimes/Tracer/PrintRunError.hpp ------------------------------===//
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

#ifndef SEEC_LIB_RUNTIMES_TRACER_PRINTRUNERROR_HPP
#define SEEC_LIB_RUNTIMES_TRACER_PRINTRUNERROR_HPP

namespace llvm {
  class Instruction;
}

namespace seec {

namespace runtime_errors {
  class RunError;
}

class AugmentationCollection;
class ModuleIndex;

namespace trace {

void PrintRunError(seec::runtime_errors::RunError const &Error,
                   llvm::Instruction const *Instruction,
                   seec::ModuleIndex const &ModIndex,
                   seec::AugmentationCollection const &Augmentations);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_LIB_RUNTIMES_TRACER_PRINTRUNERROR_HPP
