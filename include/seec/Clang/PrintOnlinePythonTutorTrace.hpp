//===- tools/seec-trace-print/OnlinePythonTutor.hpp -----------------------===//
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

#ifndef SEEC_CLANG_PRINTONLINEPYTHONTUTORTRACE_HPP
#define SEEC_CLANG_PRINTONLINEPYTHONTUTORTRACE_HPP

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

namespace seec {

class AugmentationCollection;

namespace cm {

class ProcessTrace;

class OPTSettings {
  seec::AugmentationCollection const &Augmentations;

  bool PyCrazyMode;

  std::string VariableName;

public:
  OPTSettings(seec::AugmentationCollection const &WithAugmentations)
  : Augmentations(WithAugmentations),
    PyCrazyMode(false),
    VariableName()
  {}

  seec::AugmentationCollection const &getAugmentations() const {
    return Augmentations;
  }

  bool getPyCrazyMode() const { return PyCrazyMode; }

  OPTSettings &setPyCrazyMode(bool const Value) {
    PyCrazyMode = Value;
    return *this;
  }

  std::string const &getVariableName() const { return VariableName; }

  OPTSettings &setVariableName(llvm::StringRef Value) {
    VariableName = Value;
    return *this;
  }
};

void PrintOnlinePythonTutor(seec::cm::ProcessTrace const &Trace,
                            OPTSettings const &Settings,
                            llvm::raw_ostream &Out);

void PrintOnlinePythonTutor(seec::cm::ProcessTrace const &Trace,
                            OPTSettings const &Settings);

} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_PRINTONLINEPYTHONTUTORTRACE_HPP
