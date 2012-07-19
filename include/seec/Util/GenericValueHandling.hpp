//===- seec/Util/GenericValueHandling.hpp --------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_GENERIC_VALUE_HANDLING_HPP_
#define _SEEC_UTIL_GENERIC_VALUE_HANDLING_HPP_

namespace llvm {

struct GenericValue;
class Type;
class raw_fd_ostream;
class raw_ostream;

}

namespace seec {

void printGenericValueAsType(llvm::raw_ostream &Out,
                             llvm::GenericValue const * GV,
                             llvm::Type const * Ty);

void writeGenericValueAsType(llvm::raw_fd_ostream &Out,
                             llvm::GenericValue const *GV,
                             llvm::Type const *Ty);

char const *readGenericValueOfType(char const *Start,
                                   char const *End,
                                   llvm::Type const *Ty,
                                   llvm::GenericValue &Out);
} // namespace seec

#endif // _SEEC_UTIL_GENERIC_VALUE_HANDLING_HPP_
