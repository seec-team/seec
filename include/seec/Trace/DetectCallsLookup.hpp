//===- seec/Trace/DetectCallsLookup.hpp ----------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_DETECTCALLSLOOKUP_HPP
#define SEEC_TRACE_DETECTCALLSLOOKUP_HPP

#include "llvm/ADT/StringRef.h"

/// SeeC's root namespace.
namespace seec {

/// SeeC's trace-related functionality.
namespace trace {

/// Holds implementation details for detectCalls.
namespace detect_calls {

/// Lists all detectable calls and groups.
enum class Call {
  /// @{
  /// Detectable C function call or group.
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS) PREFIX ## NAME,
#define DETECT_CALL_GROUP(PREFIX, NAME, ...) PREFIX ## NAME,
#include "DetectCallsAll.def"
  highest
  /// @}
};

/// Store the run-time locations of functions known to DetectCall.
class Lookup {
private:
  void const *Addresses[(unsigned)Call::highest];

public:
  /// Contruct a new Lookup.
  Lookup()
  : Addresses{} // Zero-initialize.
  {}

  // Documented in DetectCalls.cpp
  bool Check(Call C, void const *Address) const;

  // Documented in DetectCalls.cpp
  void const *Get(Call C) const;

  // Documented in DetectCalls.cpp
  void const *Get(llvm::StringRef Name) const;

  // Documented in DetectCalls.cpp
  void Set(llvm::StringRef Name, void const *Address);
};

} // namespace detect_calls (in trace in seec)

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_DETECTCALLSLOOKUP_HPP
