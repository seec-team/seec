#include "TraceThreadMemCheck.hpp"

#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Trace/TraceThreadListener.hpp"

#include "llvm/DerivedTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Type.h"

#include <cassert>

namespace seec {

namespace trace {

// DetectCalls Notifications

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

    // record memset 0
    auto AddressUIntPtr = static_cast<uintptr_t>(Address);
    recordUntypedState(reinterpret_cast<char const *>(AddressUIntPtr),
                       Num * Size);
  }

  // TODO: write event for failed Malloc?
}

void TraceThreadListener::preCfree(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   void *Address) {
  acquireGlobalMemoryWriteLock();
  acquireDynamicMemoryLock();

  uint64_t Address64 = reinterpret_cast<uintptr_t>(Address);

  // TODO: add Instruction (or PreInstruction) record for Call
  if (!ProcessListener.isCurrentDynamicMemoryAllocation(Address64)) {
    using namespace seec::runtime_errors;


    handleRunError(
      createRunError<RunErrorType::BadDynamicMemoryAddress>(
        format_selects::DynamicMemoryFunction::Free,
        Address64),
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

void TraceThreadListener::preCrealloc(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Address,
                                      uint64_t Size) {
  acquireGlobalMemoryWriteLock();
  acquireDynamicMemoryLock();

  uint64_t Address64 = reinterpret_cast<uintptr_t>(Address);

  if (!ProcessListener.isCurrentDynamicMemoryAllocation(Address64)) {
    using namespace seec::runtime_errors;

    handleRunError(
      createRunError<RunErrorType::BadDynamicMemoryAddress>(
        format_selects::DynamicMemoryFunction::Realloc,
        Address64),
      RunErrorSeverity::Fatal,
      Index);
  }
}

void TraceThreadListener::postCrealloc(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       void *Address,
                                       uint64_t Size) {
  auto &RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);

  assert(RTValue.assigned() && "Expected assigned RTValue.");

  auto NewAddress = RTValue.getUInt64();

  uint64_t OldAddress = reinterpret_cast<uintptr_t>(Address);

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

void TraceThreadListener::preCmemchr(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void const *Ptr,
                                     int Value,
                                     size_t Num) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  uint64_t Address = reinterpret_cast<uintptr_t>(Ptr);

  checkMemoryAccess<format_selects::MemoryAccess::Read>(*this,
                                                        Address,
                                                        Num,
                                                        Index);
}

void TraceThreadListener::preCmemcmp(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void const *Address1,
                                     void const *Address2,
                                     size_t Size) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryReadLock();

  uint64_t Address1UInt = reinterpret_cast<uintptr_t>(Address1);
  uint64_t Address2UInt = reinterpret_cast<uintptr_t>(Address2);

  checkMemoryAccess<format_selects::MemoryAccess::Read>(*this,
                                                        Address1UInt,
                                                        Size,
                                                        Index);

  checkMemoryAccess<format_selects::MemoryAccess::Read>(*this,
                                                        Address2UInt,
                                                        Size,
                                                        Index);
}

void TraceThreadListener::postCmemcmp(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void const *Address1,
                                      void const *Address2,
                                      size_t Size) {
}

void TraceThreadListener::preCmemcpy(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void *Destination,
                                     void const *Source,
                                     size_t Size) {
  using namespace seec::runtime_errors;

  acquireGlobalMemoryWriteLock();

  uint64_t DestAddr = reinterpret_cast<uintptr_t>(Destination);
  uint64_t SrcAddr = reinterpret_cast<uintptr_t>(Source);

  checkMemoryAccess<format_selects::MemoryAccess::Read>(*this,
                                                        SrcAddr,
                                                        Size,
                                                        Index);

  checkMemoryAccess<format_selects::MemoryAccess::Write>(*this,
                                                         DestAddr,
                                                         Size,
                                                         Index);

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
  recordUntypedState(reinterpret_cast<char const *>(Destination), Size);
}

void TraceThreadListener::preCmemmove(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      void const *Source,
                                      size_t Size) {
  using namespace seec::runtime_errors;

  acquireGlobalMemoryWriteLock();

  uint64_t DestAddr = reinterpret_cast<uintptr_t>(Destination);
  uint64_t SrcAddr = reinterpret_cast<uintptr_t>(Source);

  checkMemoryAccess<format_selects::MemoryAccess::Read>(*this,
                                                        SrcAddr,
                                                        Size,
                                                        Index);

  checkMemoryAccess<format_selects::MemoryAccess::Write>(*this,
                                                         DestAddr,
                                                         Size,
                                                         Index);
}

void TraceThreadListener::postCmemmove(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       void *Destination,
                                       void const *Source,
                                       size_t Size) {
  recordUntypedState(reinterpret_cast<char const *>(Destination), Size);
}

void TraceThreadListener::preCmemset(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void *Destination,
                                     int Value,
                                     size_t Size) {
  using namespace seec::runtime_errors;

  acquireGlobalMemoryWriteLock();

  uint64_t Address = reinterpret_cast<uintptr_t>(Destination);

  checkMemoryAccess<format_selects::MemoryAccess::Write>(*this,
                                                         Address,
                                                         Size,
                                                         Index);
}

void TraceThreadListener::postCmemset(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      int Value,
                                      size_t Size) {
  // TODO: if we make a memset record type, we could save a lot of data storage
  recordUntypedState(reinterpret_cast<char const *>(Destination), Size);
}

void TraceThreadListener::preCstrcat(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Destination,
                                     char const *Source) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryWriteLock();

  uint64_t const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  uint64_t const SrcAddr = reinterpret_cast<uintptr_t>(Source);

  // Check if Source points to owned memory.
  auto const SrcArea = seec::trace::getContainingMemoryArea(*this, SrcAddr);
  if (checkMemoryOwnership(*this,
                           Index,
                           SrcAddr,
                           1,
                           format_selects::MemoryAccess::Read,
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
  checkMemoryAccess(*this,
                    Index,
                    SrcAddr,
                    SrcStrLength,
                    format_selects::MemoryAccess::Read,
                    SrcArea.get<0>());

  // Check if Destination points to owned memory.
  auto const DestArea = seec::trace::getContainingMemoryArea(*this, DestAddr);
  if (checkMemoryOwnership(*this,
                           Index,
                           DestAddr,
                           1,
                           format_selects::MemoryAccess::Write,
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
  checkMemoryAccess(*this,
                    Index,
                    DestStrArea.get<0>().last(),
                    SrcStrLength,
                    format_selects::MemoryAccess::Write,
                    DestArea.get<0>());
}

void TraceThreadListener::postCstrcat(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source) {
  // Memory has been locked since the pre, so we know strlen is safe.
  auto const SrcStrLength = std::strlen(Source) + 1;
  auto const DestStrLength = std::strlen(Destination) + 1;
  auto const UnchangedChars = DestStrLength - SrcStrLength;
  recordUntypedState(Destination + UnchangedChars, SrcStrLength);
}

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

void TraceThreadListener::preCstrcpy(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Destination,
                                     char const *Source) {
  using namespace seec::runtime_errors;
  
  acquireGlobalMemoryWriteLock();

  uint64_t const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  uint64_t const SrcAddr = reinterpret_cast<uintptr_t>(Source);

  // Check if Source points to owned memory.
  auto const SrcArea = seec::trace::getContainingMemoryArea(*this, SrcAddr);
  if (checkMemoryOwnership(*this,
                           Index,
                           SrcAddr,
                           1,
                           format_selects::MemoryAccess::Read,
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
  checkMemoryAccess(*this,
                    Index,
                    SrcAddr,
                    SrcStrLength,
                    format_selects::MemoryAccess::Read,
                    SrcArea.get<0>());

  // Check if writing to Destination is OK.
  checkMemoryAccess<format_selects::MemoryAccess::Write>(*this,
                                                         DestAddr,
                                                         SrcStrLength,
                                                         Index);
}

void TraceThreadListener::postCstrcpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source) {
  // Memory has been locked since the pre, so we know strlen is safe.
  auto const SrcStrLength = std::strlen(Source) + 1;
  recordUntypedState(Destination, SrcStrLength);
}

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

void TraceThreadListener::postCstrerror(llvm::CallInst const *Call,
                                        uint32_t Index,
                                        int Errnum) {
  // TODO: Update knowledge of the ownership of the statically allocated 
  // c string returned by strerror.
}

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

void TraceThreadListener::preCstrtok(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Str,
                                     char const *Delimiters) {
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
}

void TraceThreadListener::postCstrtok(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Str,
                                      char const *Delimiters) {
  // Todo: update Str for the new NULL-character (if necessary).
}

void TraceThreadListener::preCstrxfrm(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Num) {
}

} // namespace trace (in seec)

} // namespace seec
