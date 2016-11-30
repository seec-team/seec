//===- tools/seec-trace-print/ClangMapped.hpp -----------------------------===//
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

#ifndef SEEC_TRACE_PRINT_CLANGMAPPED_HPP
#define SEEC_TRACE_PRINT_CLANGMAPPED_HPP

#include "llvm/ADT/StringRef.h"

namespace seec {
  class AugmentationCollection;
}

void PrintClangMapped(seec::AugmentationCollection const &Augmentations,
                      llvm::StringRef OPTVariableName);

#endif // SEEC_TRACE_PRINT_CLANGMAPPED_HPP
