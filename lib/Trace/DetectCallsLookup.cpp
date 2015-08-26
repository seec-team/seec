//===- lib/Trace/DetectCallsLookup.cpp ------------------------------------===//
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

#include "seec/Trace/DetectCalls.hpp"

#include "llvm/ADT/StringRef.h"

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <dlfcn.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

namespace seec {

namespace trace {

namespace detect_calls {


static void *getAddressOfSymbol(char const * const Name) {
#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
  return dlsym(RTLD_DEFAULT, Name);
#elif defined(_WIN32)
  return (void *)GetProcAddress(GetModuleHandle(nullptr), Name);
#endif
}


Lookup::Lookup()
: AddressMap{}
{
  // Find all the standard library functions we know about.
  void *Ptr;

#define DETECT_CALL(PREFIX, NAME, ARGTYPES) \
  if ((Ptr = getAddressOfSymbol(#NAME)))    \
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

seec::Maybe<Call> Lookup::Check(void const *Address) const {
  auto It = AddressMap.find(Address);
  if (It != AddressMap.end())
    return It->second;
  
  return seec::Maybe<Call>();
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
