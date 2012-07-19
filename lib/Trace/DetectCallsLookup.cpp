//===- lib/Trace/DetectCalls.cpp ------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Trace/DetectCalls.hpp"

#include "llvm/ADT/StringRef.h"

namespace seec {

namespace trace {

namespace detect_calls {

/// Check if a function is located at a certain address.
/// \param C the function to check.
/// \param Address the address to check.
/// \return true iff the function represented by C is located at Address.
bool Lookup::Check(Call C, void const *Address) const {
  return Addresses[(unsigned)C] == Address;
}

/// Get the run-time location of a function, if it is known.
/// \param C the function to locate.
/// \return the run-time location of C, or nullptr if it is unknown.
void const *Lookup::Get(Call C) const {
  return Addresses[(unsigned)C];
}

/// Get the run-time location of a function, if it is known.
/// \param Name the name of the function to locate.
/// \return the run-time location of the function, or nullptr if it is unknown.
void const *Lookup::Get(llvm::StringRef Name) const {
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS) \
  if (Name == #NAME) \
    return Addresses[(unsigned)Call::PREFIX ## NAME];
#include "seec/Trace/DetectCallsAll.def"
  return nullptr;
}

/// Set the run-time location of a function, if it is detectable by DetectCall.
/// \param Name the name of the function.
/// \param Address the run-time location of the function.
void Lookup::Set(llvm::StringRef Name, void const *Address) {
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS) \
  if (Name == #NAME) { \
    Addresses[(unsigned)Call::PREFIX ## NAME] = Address; \
    return; \
  }
#include "seec/Trace/DetectCallsAll.def"
}

} // namespace detect_calls

} // namespace trace

} // namespace seec
