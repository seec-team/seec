//===- include/seec/Clang/SubRangeRecorder.hpp ----------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_CLANG_SUBRANGERECORDER_HPP
#define SEEC_CLANG_SUBRANGERECORDER_HPP

#include "llvm/ADT/DenseMap.h"

#include <utility>

namespace llvm {
  class raw_ostream;
}
namespace clang {
  struct PrintingPolicy;
  class Stmt;
}

namespace seec {

class PrintedStmtRange
{
  uint64_t Start;

  std::size_t Length;

public:
  PrintedStmtRange(uint64_t WithStart, std::size_t WithLength)
  : Start(WithStart),
    Length(WithLength)
  {}

  uint64_t getStart() const { return Start; }

  std::size_t getLength() const { return Length; }
};

llvm::DenseMap<clang::Stmt *, PrintedStmtRange>
printStmtAndRecordRanges(llvm::raw_ostream &OS,
                         clang::Stmt const *E,
                         clang::PrintingPolicy &Policy);

} // namespace seec

#endif // SEEC_CLANG_SUBRANGERECORDER_HPP
