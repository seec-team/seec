#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Trace/TraceThreadListener.hpp"

#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>

#include "TraceThreadMemCheck.hpp"

namespace seec {

namespace trace {

//===------------------------------------------------------------------------===
// DetectCalls Notifications
//===------------------------------------------------------------------------===


//===------------------------------------------------------------------------===
// atof
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatof(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   char const *Str) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Atof,
                   0, // Parameter Index for Str.
                   Str);
}


//===------------------------------------------------------------------------===
// atoi
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatoi(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   char const *Str) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Atoi,
                   0, // Parameter Index for Str.
                   Str);
}


//===------------------------------------------------------------------------===
// atol
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatol(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   char const *Str) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Atol,
                   0, // Parameter Index for Str.
                   Str);
}


//===------------------------------------------------------------------------===
// strtod
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtod(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str,
                                     char **EndPtr) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strtod,
                   0, // Parameter Index for Str.
                   Str);
  
  if (EndPtr) {
    // TODO: Check if EndPtr is valid.
  }
}


//===------------------------------------------------------------------------===
// strtol
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtol(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str,
                                     char **EndPtr,
                                     int Base) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strtol,
                   0, // Parameter Index for Str.
                   Str);
  
  if (EndPtr) {
    // TODO: Check if EndPtr is valid.
  }
}


//===------------------------------------------------------------------------===
// strtoul
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtoul(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      char **EndPtr,
                                      int Base) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strtoul,
                   0, // Parameter Index for Str.
                   Str);
  
  if (EndPtr) {
    // TODO: Check if EndPtr is valid.
  }
}


//===------------------------------------------------------------------------===
// calloc
//===------------------------------------------------------------------------===

void TraceThreadListener::preCcalloc(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     size_t Num,
                                     size_t Size) {
  acquireGlobalMemoryWriteLock();
  acquireDynamicMemoryLock();
}

void TraceThreadListener::postCcalloc(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      size_t Num,
                                      size_t Size) {
  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);

  assert(RTValue.assigned() && "Expected assigned RTValue.");

  auto Address = RTValue.getUInt64();

  if (Address) {
    recordMalloc(Address, Num * Size);

    // Record memset 0 because calloc will clear the memory.
    auto AddressUIntPtr = static_cast<uintptr_t>(Address);
    recordUntypedState(reinterpret_cast<char const *>(AddressUIntPtr),
                       Num * Size);
  }

  // TODO: write event for failed Malloc?
}


//===------------------------------------------------------------------------===
// free
//===------------------------------------------------------------------------===

void TraceThreadListener::preCfree(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   void *Address) {
  acquireGlobalMemoryWriteLock();
  acquireDynamicMemoryLock();

  auto AddressInt = reinterpret_cast<uintptr_t>(Address);

  if (!ProcessListener.isCurrentDynamicMemoryAllocation(AddressInt)) {
    using namespace seec::runtime_errors;


    handleRunError(
      createRunError<RunErrorType::BadDynamicMemoryAddress>(
        format_selects::DynamicMemoryFunction::Free,
        AddressInt),
      RunErrorSeverity::Fatal,
      Index);
  }
}

void TraceThreadListener::postCfree(llvm::CallInst const *Call,
                                    uint32_t Index,
                                    void *Address) {
  EventsOut.write<EventType::Instruction>(Index, ++Time);
  
  auto FreedMalloc = recordFree(reinterpret_cast<uintptr_t>(Address));
  
  recordStateClear(reinterpret_cast<uintptr_t>(Address), FreedMalloc.size());
}


//===------------------------------------------------------------------------===
// malloc
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmalloc(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     size_t Size) {
  acquireDynamicMemoryLock();
}

void TraceThreadListener::postCmalloc(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      size_t Size) {
  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);

  assert(RTValue.assigned() && "Expected assigned RTValue.");

  auto Address = RTValue.getUInt64();

  if (Address) {
    recordMalloc(Address, Size);
  }
}


//===------------------------------------------------------------------------===
// realloc
//===------------------------------------------------------------------------===

void TraceThreadListener::preCrealloc(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Address,
                                      size_t Size) {
  acquireGlobalMemoryWriteLock();
  acquireDynamicMemoryLock();

  auto AddressInt = reinterpret_cast<uintptr_t>(Address);

  if (!ProcessListener.isCurrentDynamicMemoryAllocation(AddressInt)) {
    using namespace seec::runtime_errors;

    handleRunError(
      createRunError<RunErrorType::BadDynamicMemoryAddress>(
        format_selects::DynamicMemoryFunction::Realloc,
        AddressInt),
      RunErrorSeverity::Fatal,
      Index);
  }
}

void TraceThreadListener::postCrealloc(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       void *Address,
                                       size_t Size) {
  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);

  assert(RTValue.assigned() && "Expected assigned RTValue.");

  auto NewAddress = RTValue.getUInt64();

  auto OldAddress = reinterpret_cast<uintptr_t>(Address);

  if (OldAddress) {
    if (Size) {
      if (NewAddress) {
        if (NewAddress == OldAddress) {
          // Record free first, so that when we reverse over the events, the
          // freed malloc will be recreated after the new malloc is removed.
          auto FreedMalloc = recordFree(OldAddress);

          // If this realloc shrank the allocation, then clear the memory that
          // is no longer allocated.
          if (Size < FreedMalloc.size()) {
            recordStateClear(NewAddress + Size, FreedMalloc.size() - Size);
          }

          // Record malloc for the new size.
          recordMalloc(NewAddress, Size);
        }
        else {
          // Malloc new address.
          recordMalloc(NewAddress, Size);

          // TODO: Change to memcpy when implemented.
          recordUntypedState(reinterpret_cast<char const *>(NewAddress), Size);

          // Free previous address and clear the memory.
          recordFreeAndClear(OldAddress);
        }
      }
    }
    else { // Size is 0: behave as free
      recordFreeAndClear(OldAddress);
    }
  }
  else { // Address is NULL: behave as malloc
    if (NewAddress) {
      recordMalloc(NewAddress, Size);
    }
  }
}


//===------------------------------------------------------------------------===
// getenv
//===------------------------------------------------------------------------===

void TraceThreadListener::preCgetenv(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Name) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Getenv,
                   0, // Parameter Index for Name.
                   Name);
}

void TraceThreadListener::postCgetenv(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Name) {
  // Get the pointer returned by getenv.
  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue.assigned() && "Expected assigned RTValue.");
  
  auto Address64 = RTValue.getUInt64();
  if (!Address64)
    return;
  
  auto Str = reinterpret_cast<char const *>(static_cast<uintptr_t>(Address64));
  auto Length = std::strlen(Str) + 1; // Include terminating nul byte.
  
  // Remove knowledge of the existing getenv string at this position (if any).
  ProcessListener.removeKnownMemoryRegion(Address64);
  
  // TODO: Delete any existing memory states at this address.
  
  // Set knowledge of the new string area.
  ProcessListener.addKnownMemoryRegion(Address64,
                                       Length,
                                       MemoryPermission::ReadOnly);
  
  // Set the new string at this address.
  recordUntypedState(Str, Length);
}


//===------------------------------------------------------------------------===
// system
//===------------------------------------------------------------------------===

void TraceThreadListener::preCsystem(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Command) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  // A NULL Command is valid, so only check if it is non-null.
  if (Command) {
    checkCStringRead(*this,
                     Index,
                     format_selects::StringFunction::System,
                     0, // Parameter Index for Command.
                     Command);
  }
}


//===------------------------------------------------------------------------===
// memchr
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemchr(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void const *Ptr,
                                     int Value,
                                     size_t Num) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  auto Address = reinterpret_cast<uintptr_t>(Ptr);
  
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memchr,
                               0, // Ptr is parameter 0
                               format_selects::MemoryAccess::Read,
                               Address,
                               Num);
}


//===------------------------------------------------------------------------===
// memcmp
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemcmp(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void const *Address1,
                                     void const *Address2,
                                     size_t Size) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  auto Address1Int = reinterpret_cast<uintptr_t>(Address1);
  auto Address2Int = reinterpret_cast<uintptr_t>(Address2);

  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memcmp,
                               0, // Address1 is parameter 0
                               format_selects::MemoryAccess::Read,
                               Address1Int,
                               Size);

  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memcmp,
                               1, // Address2 is parameter 1
                               format_selects::MemoryAccess::Read,
                               Address2Int,
                               Size);
}

void TraceThreadListener::postCmemcmp(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void const *Address1,
                                      void const *Address2,
                                      size_t Size) {
}


//===------------------------------------------------------------------------===
// memcpy
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemcpy(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void *Destination,
                                     void const *Source,
                                     size_t Size) {
  using namespace seec::runtime_errors;

  acquireGlobalMemoryWriteLock();

  auto DestAddr = reinterpret_cast<uintptr_t>(Destination);
  auto SrcAddr = reinterpret_cast<uintptr_t>(Source);

  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memcpy,
                               1, // Source is parameter 1
                               format_selects::MemoryAccess::Read,
                               SrcAddr,
                               Size);
  
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memcpy,
                               0, // Destination is parameter 0
                               format_selects::MemoryAccess::Write,
                               DestAddr,
                               Size);

  checkMemoryOverlap<format_selects::MemCopyFunction::Memcpy>(
    *this,
    Index,
    MemoryArea(DestAddr, Size),
    MemoryArea(SrcAddr, Size));
}

void TraceThreadListener::postCmemcpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      void const *Source,
                                      size_t Size) {
  recordMemmove(reinterpret_cast<uintptr_t>(Source),
                reinterpret_cast<uintptr_t>(Destination),
                Size);
}


//===------------------------------------------------------------------------===
// memmove
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemmove(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      void const *Source,
                                      size_t Size) {
  using namespace seec::runtime_errors;

  acquireGlobalMemoryWriteLock();

  auto DestAddr = reinterpret_cast<uintptr_t>(Destination);
  auto SrcAddr = reinterpret_cast<uintptr_t>(Source);

  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memmove,
                               1, // Source is parameter 1
                               format_selects::MemoryAccess::Read,
                               SrcAddr,
                               Size);
  
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memmove,
                               0, // Destination is parameter 0
                               format_selects::MemoryAccess::Write,
                               DestAddr,
                               Size);
}

void TraceThreadListener::postCmemmove(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       void *Destination,
                                       void const *Source,
                                       size_t Size) {
  recordMemmove(reinterpret_cast<uintptr_t>(Source),
                reinterpret_cast<uintptr_t>(Destination),
                Size);
}


//===------------------------------------------------------------------------===
// memset
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemset(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void *Destination,
                                     int Value,
                                     size_t Size) {
  using namespace seec::runtime_errors;

  acquireGlobalMemoryWriteLock();

  auto Address = reinterpret_cast<uintptr_t>(Destination);

  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Memset,
                               0, // Destination is parameter 0
                               format_selects::MemoryAccess::Write,
                               Address,
                               Size);
}

void TraceThreadListener::postCmemset(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      int Value,
                                      size_t Size) {
  recordUntypedState(reinterpret_cast<char const *>(Destination), Size);
}


//===------------------------------------------------------------------------===
// strcat
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcat(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Destination,
                                     char const *Source) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryWriteLock();

  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  auto const SrcAddr = reinterpret_cast<uintptr_t>(Source);

  // Check if Source points to owned memory.
  auto const SrcArea = seec::trace::getContainingMemoryArea(*this, SrcAddr);
  if (checkMemoryOwnershipOfParameter(*this,
                                      Index,
                                      format_selects::StandardFunction::Strcat,
                                      1, // Source is parameter 1
                                      format_selects::MemoryAccess::Read,
                                      SrcAddr,
                                      1, // Read at least 1 byte
                                      SrcArea)) {
    return;
  }

  // Check if Source points to a valid C string.
  auto const SrcStrArea = getCStringInArea(Source, SrcArea.get<0>());
  if (checkCStringIsValid(*this,
                          Index,
                          SrcAddr,
                          1, // Parameter Index for Source.
                          format_selects::StringFunction::Strcat,
                          SrcStrArea)) {
    return;
  }

  auto const SrcStrLength = SrcStrArea.get<0>().length();

  // Check if the read from Source is OK. We already know that the size of the
  // read is valid, from using getCStringInArea, but this will check if the
  // memory is initialized.
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Strcat,
                               1, // Source is parameter 1
                               format_selects::MemoryAccess::Read,
                               SrcAddr,
                               SrcStrLength,
                               SrcArea.get<0>());

  // Check if Destination points to owned memory.
  auto const DestArea = seec::trace::getContainingMemoryArea(*this, DestAddr);
  if (checkMemoryOwnershipOfParameter(*this,
                                      Index,
                                      format_selects::StandardFunction::Strcat,
                                      0, // Destination is parameter 0
                                      format_selects::MemoryAccess::Write,
                                      DestAddr,
                                      1, // Access at least 1 byte
                                      DestArea)) {
    return;
  }

  // Check if Destination points to a valid C string.
  auto const DestStrArea = getCStringInArea(Destination, DestArea.get<0>());
  if (checkCStringIsValid(*this,
                          Index,
                          DestAddr,
                          0, // Parameter Index for Destination.
                          format_selects::StringFunction::Strcat,
                          DestStrArea)) {
    return;
  }

  // Check if it is OK to write the Source string to the end of the Destination
  // string.
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Strcat,
                               0, // Destination is parameter 0
                               format_selects::MemoryAccess::Write,
                               DestStrArea.get<0>().last(),
                               SrcStrLength,
                               DestArea.get<0>());
}

void TraceThreadListener::postCstrcat(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source) {
  // Memory has been locked since the pre, so strlen should be safe.
  auto const SrcStrLength = std::strlen(Source) + 1;
  auto const DestStrLength = std::strlen(Destination) + 1;
  auto const UnchangedChars = DestStrLength - SrcStrLength;
  recordUntypedState(Destination + UnchangedChars, SrcStrLength);
}


//===------------------------------------------------------------------------===
// strchr
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrchr(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str,
                                     int Character) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strchr,
                   0, // Parameter Index for Str.
                   Str);
}


//===------------------------------------------------------------------------===
// strcmp
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcmp(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str1,
                                     char const *Str2) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strcmp,
                   0, // Parameter Index for Str1.
                   Str1);

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strcmp,
                   1, // Parameter Index for Str2.
                   Str2);
}


//===------------------------------------------------------------------------===
// strcoll
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcoll(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strcoll,
                   0, // Parameter Index for Str1.
                   Str1);

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strcoll,
                   1, // Parameter Index for Str2.
                   Str2);
}


//===------------------------------------------------------------------------===
// strcpy
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcpy(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Destination,
                                     char const *Source) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryWriteLock();

  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  auto const SrcAddr = reinterpret_cast<uintptr_t>(Source);

  // Check if Source points to owned memory.
  auto const SrcArea = seec::trace::getContainingMemoryArea(*this, SrcAddr);
  if (checkMemoryOwnershipOfParameter(*this,
                                      Index,
                                      format_selects::StandardFunction::Strcpy,
                                      1, // Source is parameter 1
                                      format_selects::MemoryAccess::Read,
                                      SrcAddr,
                                      1, // Read at least one byte
                                      SrcArea)) {
    return;
  }

  // Check if Source points to a valid C string.
  auto const SrcStrArea = getCStringInArea(Source, SrcArea.get<0>());
  if (checkCStringIsValid(*this,
                          Index,
                          SrcAddr,
                          1, // Parameter Index for Source.
                          format_selects::StringFunction::Strcpy,
                          SrcStrArea)) {
    return;
  }

  auto const SrcStrLength = SrcStrArea.get<0>().length();

  // Check if the read from Source is OK. We already know that the size of the
  // read is valid, from using getCStringInArea, but this will check if the
  // memory is initialized.
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Strcpy,
                               1, // Source is parameter 1
                               format_selects::MemoryAccess::Read,
                               SrcAddr,
                               SrcStrLength,
                               SrcArea.get<0>());

  // Check if writing to Destination is OK.
  checkMemoryAccessOfParameter(*this,
                               Index,
                               format_selects::StandardFunction::Strcpy,
                               0, // Destination is parameter 0
                               format_selects::MemoryAccess::Write,
                               DestAddr,
                               SrcStrLength);
}

void TraceThreadListener::postCstrcpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source) {
  // Memory has been locked since the pre, so we know strlen is safe.
  auto const SrcStrLength = std::strlen(Source) + 1;
  recordUntypedState(Destination, SrcStrLength);
}


//===------------------------------------------------------------------------===
// strcspn
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcspn(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strspn,
                   0, // Parameter Index for Str1.
                   Str1);
  
  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strspn,
                   1, // Parameter Index for Str2.
                   Str2);
}


//===------------------------------------------------------------------------===
// strerror
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrerror(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       int Errnum) {
  acquireGlobalMemoryWriteLock();
}

void TraceThreadListener::postCstrerror(llvm::CallInst const *Call,
                                        uint32_t Index,
                                        int Errnum) {
  // Get the pointer returned by strerror.
  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue.assigned() && "Expected assigned RTValue.");
  
  auto Address64 = RTValue.getUInt64();
  auto Str = reinterpret_cast<char const *>(static_cast<uintptr_t>(Address64));
  auto Length = std::strlen(Str) + 1; // Include terminating nul byte.
  
  // Remove knowledge of the existing strerror string (if any).
  ProcessListener.removeKnownMemoryRegion(Address64);
  
  // TODO: Delete any existing memory states at this address.
  
  // Set knowledge of the new string area.
  ProcessListener.addKnownMemoryRegion(Address64,
                                       Length,
                                       MemoryPermission::ReadOnly);
  
  // Set the new string at this address.
  recordUntypedState(Str, Length);
}


//===------------------------------------------------------------------------===
// strlen
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrlen(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strlen,
                   0, // Parameter Index for Str.
                   Str);
}


//===------------------------------------------------------------------------===
// strncat
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrncat(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Size) {
  acquireGlobalMemoryWriteLock();
}

void TraceThreadListener::postCstrncat(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char *Destination,
                                       char const *Source,
                                       size_t Size) {
}


//===------------------------------------------------------------------------===
// strncmp
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrncmp(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2,
                                      size_t Num) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkLimitedCStringRead(*this,
                          Index,
                          format_selects::StringFunction::Strncmp,
                          0, // Parameter Index for Str1.
                          Str1,
                          Num);

  checkLimitedCStringRead(*this,
                          Index,
                          format_selects::StringFunction::Strncmp,
                          1, // Parameter Index for Str2.
                          Str2,
                          Num);
}


//===------------------------------------------------------------------------===
// strncpy
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrncpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Size) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryWriteLock();
  
  MemoryArea DestArea (Destination, Size);
  MemoryArea SrcArea (Source, Size);
  
  checkMemoryOverlap<format_selects::MemCopyFunction::Strncpy>(*this,
                                                               Index,
                                                               DestArea,
                                                               SrcArea);
}

void TraceThreadListener::postCstrncpy(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char *Destination,
                                       char const *Source,
                                       size_t Size) {
}


//===------------------------------------------------------------------------===
// strpbrk
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrpbrk(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strpbrk,
                   0, // Parameter Index for Str1.
                   Str1);
  
  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strpbrk,
                   1, // Parameter Index for Str2.
                   Str2);
}


//===------------------------------------------------------------------------===
// strrchr
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrrchr(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      int Character) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strrchr,
                   0, // Parameter Index for Str.
                   Str);
}


//===------------------------------------------------------------------------===
// strspn
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrspn(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str1,
                                     char const *Str2) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strspn,
                   0, // Parameter Index for Str1.
                   Str1);
  
  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strspn,
                   1, // Parameter Index for Str2.
                   Str2);
}


//===------------------------------------------------------------------------===
// strstr
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrstr(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str1,
                                     char const *Str2) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strstr,
                   0, // Parameter Index for Str1.
                   Str1);
  
  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strstr,
                   1, // Parameter Index for Str2.
                   Str2);
}


//===------------------------------------------------------------------------===
// strtok
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtok(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Str,
                                     char const *Delimiters) {
  llvm::errs() << "strtok is not supported\n";
  exit(EXIT_FAILURE);
  
#if 0
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryWriteLock();
  
  if (Str == NULL) {
    // TODO: Str may legitimately be NULL if strtok has been called previously.
  }
  else {
    // TODO: Check reading/writing Str.
  }
  
  checkCStringRead(*this,
                   Index,
                   format_selects::StringFunction::Strtok,
                   1, // Parameter Index for Delimiters.
                   Str);
#endif
}

void TraceThreadListener::postCstrtok(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Str,
                                      char const *Delimiters) {
  // Todo: update Str for the new NULL-character (if necessary).
}


//===------------------------------------------------------------------------===
// strxfrm
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrxfrm(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Num) {
  llvm::errs() << "strxfrm is not supported\n";
  exit(EXIT_FAILURE);
}


} // namespace trace (in seec)

} // namespace seec
