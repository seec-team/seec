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

namespace seec {
  class AugmentationCollection;
}

void PrintClangMapped(seec::AugmentationCollection const &Augmentations);

#endif // SEEC_TRACE_PRINT_CLANGMAPPED_HPP
