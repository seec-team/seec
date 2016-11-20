//===- include/seec/Trace/IsRecordableType.hpp ---------------------- C++ -===//
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

namespace llvm {
  class Type;
}

namespace seec {

namespace trace {

/// \brief Check if \c TheType is recordable/replayable.
/// \param TheType the type to check.
///
bool isRecordableType(llvm::Type const *TheType);

} // namespace trace (in seec)

} // namespace seec