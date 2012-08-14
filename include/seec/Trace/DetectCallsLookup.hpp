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

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

/// SeeC's root namespace.
namespace seec {

/// SeeC's trace-related functionality.
namespace trace {

/// Holds implementation details for detectCalls.
namespace detect_calls {


/// Lists all detectable calls and groups.
enum class Call : std::size_t {
  /// @{
  /// Detectable C function call or group.
#define DETECT_CALL(PREFIX, NAME, LOCALS, ARGS) PREFIX ## NAME,
#define DETECT_CALL_GROUP(PREFIX, NAME, ...) PREFIX ## NAME,
#include "DetectCallsAll.def"
  highest
  /// @}
};


/// \brief Store the run-time locations of functions known to DetectCall.
///
/// It is not thread-safe to call Set() while other threads may be calling
/// either Set() or Check().
///
class Lookup {
private:
  llvm::DenseMap<void const *, Call> AddressMap;
  
public:
  /// Contruct a new Lookup.
  Lookup();

  /// Check if a function is located at a certain address.
  /// \param C the function to check.
  /// \param Address the address to check.
  /// \return true iff the function represented by C is located at Address.
  bool Check(Call C, void const *Address) const;
  
  /// Check if a function is located at a certain address.
  /// \param Name the name of the function to check for.
  /// \param Address the address to check.
  /// \return true iff the function located at Address is named Name.
  bool Check(llvm::StringRef Name, void const *Address) const;

  /// Set the run-time location of a function, if it is detectable by
  /// DetectCall.
  /// \param Name the name of the function.
  /// \param Address the run-time location of the function.
  /// \return true iff the function was recorded (and is now detectable).
  bool Set(llvm::StringRef Name, void const *Address);
};


} // namespace detect_calls (in trace in seec)

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_DETECTCALLSLOOKUP_HPP
