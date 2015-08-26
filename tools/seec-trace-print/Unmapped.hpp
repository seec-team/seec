//===- tools/seec-trace-print/Unmapped.hpp --------------------------------===//
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

#ifndef SEEC_TRACE_PRINT_UNMAPPED_HPP
#define SEEC_TRACE_PRINT_UNMAPPED_HPP

namespace seec {
  class AugmentationCollection;
}

void PrintUnmapped(seec::AugmentationCollection const &Augmentations);

#endif // SEEC_TRACE_PRINT_UNMAPPED_HPP
