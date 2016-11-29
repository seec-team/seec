//===- seec/Trace/GetRecreatedValue.hpp ----------------------------- C++ -===//
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

#ifndef SEEC_TRACE_GETRECREATEDVALUE_HPP
#define SEEC_TRACE_GETRECREATEDVALUE_HPP

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/Optional.h"

namespace llvm {
  class Value;
}

namespace seec {

namespace trace {

class FunctionState;

llvm::Optional<llvm::APInt>
getAPInt(FunctionState const &State, llvm::Value const *Value);

llvm::Optional<llvm::APSInt>
getAPSIntUnsigned(FunctionState const &State, llvm::Value const *Value);

llvm::Optional<llvm::APSInt>
getAPSIntSigned(FunctionState const &State, llvm::Value const *Value);

llvm::Optional<llvm::APFloat>
getAPFloat(FunctionState const &State, llvm::Value const *Value);

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_GETRECREATEDVALUE_HPP
