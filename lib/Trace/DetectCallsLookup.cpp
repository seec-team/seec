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

#include <dlfcn.h>

namespace seec {

namespace trace {

namespace detect_calls {


Lookup::Lookup()
: AddressMap{}
{
  // Find all the standard library functions we know about.
  void *Ptr;
  
#define DETECT_CALL(PREFIX, NAME, ARGTYPES) \
  if ((Ptr = dlsym(RTLD_DEFAULT, #NAME))) \
    AddressMap.insert(std::make_pair(Ptr, Call::PREFIX##NAME));
#include "seec/Trace/DetectCallsAll.def"
}

bool Lookup::Check(Call C, void const *Address) const {
  // Try to find the Call in our AddressMap first.
  auto It = AddressMap.find(Address);
  if (It != AddressMap.end())
    return It->second == C;
  
  return false;
}

seec::util::Maybe<Call> Lookup::Check(void const *Address) const {
  auto It = AddressMap.find(Address);
  if (It != AddressMap.end())
    return It->second;
  
  return seec::util::Maybe<Call>();
}

bool Lookup::Set(llvm::StringRef Name, void const *Address) {
#define DETECT_CALL(PREFIX, NAME, ARGTYPES) \
  if (Name.equals(#NAME)) { \
    AddressMap.insert(std::make_pair(Address, Call::PREFIX##NAME)); \
    return true; \
  }
#include "seec/Trace/DetectCallsAll.def"
  
  return false;
}


} // namespace detect_calls

} // namespace trace

} // namespace seec
