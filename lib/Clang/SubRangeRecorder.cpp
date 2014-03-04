//===- lib/Clang/SubRangeRecorder.cpp -------------------------------------===//
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

#include "seec/Clang/SubRangeRecorder.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/PrettyPrinter.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

/// \brief Records the range of each Stmt in a pretty-printed Stmt.
///
class SubRangeRecorder : public clang::PrinterHelper
{
  clang::PrintingPolicy &Policy;
  
  std::string Buffer;
  
  llvm::raw_string_ostream BufferOS;
  
  llvm::DenseMap<clang::Stmt *, PrintedStmtRange> Ranges;
  
public:
  SubRangeRecorder(clang::PrintingPolicy &WithPolicy)
  : Policy(WithPolicy),
    Buffer(),
    BufferOS(Buffer),
    Ranges()
  {}
  
  virtual bool handledStmt(clang::Stmt *E, llvm::raw_ostream &OS) {
    // Print the Stmt to determine the length of its printed text.
    Buffer.clear();
    E->printPretty(BufferOS, nullptr, Policy);
    BufferOS.flush();
    
    // Record the start and length of the Stmt's printed text.
    Ranges.insert(std::make_pair(E,
                                 PrintedStmtRange(OS.tell(), Buffer.size())));
    
    return false;
  }
  
  llvm::DenseMap<clang::Stmt *, PrintedStmtRange> moveRanges() {
    return std::move(Ranges);
  }
};

llvm::DenseMap<clang::Stmt *, PrintedStmtRange>
printStmtAndRecordRanges(llvm::raw_ostream &OS,
                         clang::Stmt const *E,
                         clang::PrintingPolicy &Policy)
{
  SubRangeRecorder PrinterHelper(Policy);
  E->printPretty(OS, &PrinterHelper, Policy);
  OS.flush();
  return PrinterHelper.moveRanges();
}

} // namespace seec
