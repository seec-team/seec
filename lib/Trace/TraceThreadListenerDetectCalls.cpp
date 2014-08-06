//===- lib/Trace/TraceThreadListenerDetectCalls.cpp -----------------------===//
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

#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>


namespace seec {

namespace trace {

//===------------------------------------------------------------------------===
// DetectCalls Notifications
//===------------------------------------------------------------------------===


//===------------------------------------------------------------------------===
// __ctype_b_loc
//===------------------------------------------------------------------------===

void TraceThreadListener::postLINUX__ctype_b_loc(llvm::CallInst const *Call,
                                                 uint32_t Index)
{
  // Get the pointer returned.
  auto const RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto const Address = RTValue->getUIntPtr();
  if (!Address)
    return;
  
  auto const Ptr = reinterpret_cast<const unsigned short int **>(Address);
  
  // Lock the global memory.
  acquireGlobalMemoryWriteLock();
  
  // Add knowledge of the pointer at this location.
  if (!isKnownMemoryRegionAt(Address)) {
    addKnownMemoryRegion(Address,
                         sizeof(const unsigned short int **),
                         MemoryPermission::ReadOnly);
    recordUntypedState(reinterpret_cast<char const *>(Ptr),
                       sizeof(const unsigned short int **));
  }

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));

  auto const ArrPtr = *Ptr;
  auto const ArrStart = &ArrPtr[-128];
  auto const ArrStartAddr = reinterpret_cast<uintptr_t>(ArrStart);

  if (!isKnownMemoryRegionAt(ArrStartAddr)) {
    auto const ArrSize = 384 * sizeof(*ArrPtr);
    addKnownMemoryRegion(ArrStartAddr, ArrSize, MemoryPermission::ReadOnly);
    recordUntypedState(reinterpret_cast<char const *>(ArrStart),
                       ArrSize);
  }

  ProcessListener.setInMemoryPointerObject(
    reinterpret_cast<uintptr_t>(ArrPtr),
    ProcessListener.makePointerObject(ArrStartAddr));
}


//===------------------------------------------------------------------------===
// __ctype_tolower_loc
//===------------------------------------------------------------------------===

void
TraceThreadListener::postLINUX__ctype_tolower_loc(llvm::CallInst const *Call,
                                                  uint32_t Index)
{
  // Get the pointer returned.
  auto const RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto const Address = RTValue->getUIntPtr();
  if (!Address)
    return;
  
  auto const Ptr = reinterpret_cast<const uint32_t **>(Address);
  
  // Lock the global memory.
  acquireGlobalMemoryWriteLock();
  
  // Remove knowledge of any existing pointer at this position.
  if (!isKnownMemoryRegionAt(Address)) {
    addKnownMemoryRegion(Address,
                         sizeof(const uint32_t **),
                         MemoryPermission::ReadOnly);
    recordUntypedState(reinterpret_cast<char const *>(Ptr),
                       sizeof(const uint32_t **));
  }
  
  auto const ArrPtr = *Ptr;
  auto const ArrStart = &ArrPtr[-128];
  auto const ArrStartAddr = reinterpret_cast<uintptr_t>(ArrStart);

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));

  if (!isKnownMemoryRegionAt(ArrStartAddr)) {
    auto const ArrSize = 384 * sizeof(*ArrPtr);
    addKnownMemoryRegion(ArrStartAddr, ArrSize, MemoryPermission::ReadOnly);
    recordUntypedState(reinterpret_cast<char const *>(ArrStart),
                       ArrSize);
  }

  ProcessListener.setInMemoryPointerObject(
    reinterpret_cast<uintptr_t>(ArrPtr),
    ProcessListener.makePointerObject(ArrStartAddr));
}


//===------------------------------------------------------------------------===
// __ctype_toupper_loc
//===------------------------------------------------------------------------===

void
TraceThreadListener::postLINUX__ctype_toupper_loc(llvm::CallInst const *Call,
                                                  uint32_t Index)
{
  // Get the pointer returned.
  auto const RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto const Address = RTValue->getUIntPtr();
  if (!Address)
    return;
  
  auto const Ptr = reinterpret_cast<const uint32_t **>(Address);
  
  // Lock the global memory.
  acquireGlobalMemoryWriteLock();
  
  // Remove knowledge of any existing pointer at this position.
  if (!isKnownMemoryRegionAt(Address)) {
    addKnownMemoryRegion(Address,
                         sizeof(const uint32_t **),
                         MemoryPermission::ReadOnly);
    recordUntypedState(reinterpret_cast<char const *>(Ptr),
                       sizeof(const uint32_t **));
  }
  
  auto const ArrPtr = *Ptr;
  auto const ArrStart = &ArrPtr[-128];
  auto const ArrStartAddr = reinterpret_cast<uintptr_t>(ArrStart);

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));

  if (!isKnownMemoryRegionAt(ArrStartAddr)) {
    auto const ArrSize = 384 * sizeof(*ArrPtr);
    addKnownMemoryRegion(ArrStartAddr, ArrSize, MemoryPermission::ReadOnly);
    recordUntypedState(reinterpret_cast<char const *>(ArrStart),
                       ArrSize);
  }

  ProcessListener.setInMemoryPointerObject(
    reinterpret_cast<uintptr_t>(ArrPtr),
    ProcessListener.makePointerObject(ArrStartAddr));
}


//===------------------------------------------------------------------------===
// fopen
//===------------------------------------------------------------------------===

void TraceThreadListener::preCfopen(llvm::CallInst const *Call,
                                    uint32_t Index,
                                    char const *Filename,
                                    char const *Mode) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryReadLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::fopen);
  Checker.checkCStringRead(0, Filename);
  Checker.checkCStringRead(1, Mode);
}

void TraceThreadListener::postCfopen(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Filename,
                                     char const *Mode) {
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address) {
    recordStreamOpen(reinterpret_cast<FILE *>(Address), Filename, Mode);
    ProcessListener.incrementRegionTemporalID(Address);
  }

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));
}


//===------------------------------------------------------------------------===
// freopen
//===------------------------------------------------------------------------===

void TraceThreadListener::preCfreopen(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Filename,
                                      char const *Mode,
                                      FILE *Stream) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryReadLock();
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fopen,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkCStringRead(0, Filename);
  Checker.checkCStringRead(1, Mode);
  Checker.checkStreamIsValid(2, Stream);
}

void TraceThreadListener::postCfreopen(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Filename,
                                       char const *Mode,
                                       FILE *Stream) {
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address) {
    recordStreamClose(Stream);
    recordStreamOpen(reinterpret_cast<FILE *>(Address), Filename, Mode);
  }

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));
}


//===------------------------------------------------------------------------===
// fclose
//===------------------------------------------------------------------------===

void TraceThreadListener::preCfclose(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     FILE *Stream) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fclose,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(0, Stream);
}

void TraceThreadListener::postCfclose(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      FILE *Stream)
{
  recordStreamClose(Stream);
}


//===------------------------------------------------------------------------===
// fflush
//===------------------------------------------------------------------------===

void TraceThreadListener::preCfflush(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     FILE *Stream) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fflush,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(0, Stream);
}


//===------------------------------------------------------------------------===
// fwide
//===------------------------------------------------------------------------===

void TraceThreadListener::preCfwide(llvm::CallInst const *Call,
                                    uint32_t Index,
                                    FILE *Stream,
                                    int Mode) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fwide,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(0, Stream);
}


//===------------------------------------------------------------------------===
// fgetc
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCfgetc(llvm::CallInst const *Call, uint32_t Index, FILE *Stream)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fgetc,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(0, Stream);
}


//===------------------------------------------------------------------------===
// fgets
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCfgets(llvm::CallInst const *Call,
          uint32_t Index,
          char *Str,
          int Count,
          FILE *Stream)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fgets,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(2, Stream);
  Checker.checkMemoryExistsAndAccessibleForParameter
            (0, reinterpret_cast<uintptr_t>(Str), Count, MemoryAccess::Write);
}

void
TraceThreadListener::
postCfgets(llvm::CallInst const *Call,
           uint32_t Index,
           char *Str,
           int Count,
           FILE *Stream)
{
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (!Address) {
    ActiveFunction->setPointerObject(Call, PointerTarget{});
    return;
  }

  ActiveFunction->transferArgPointerObjectToCall(0);
  recordUntypedState(Str, std::strlen(Str) + 1);
}


//===------------------------------------------------------------------------===
// fputc
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCfputc(llvm::CallInst const *Call, uint32_t Index, int Ch, FILE *Stream)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fputc,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(1, Stream);
}

void
TraceThreadListener::
postCfputc(llvm::CallInst const *Call, uint32_t Index, int Ch, FILE *Stream)
{
  char C = Ch;
  recordStreamWrite(Stream, llvm::ArrayRef<char>(&C, 1));
}


//===------------------------------------------------------------------------===
// fputs
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCfputs(llvm::CallInst const *Call,
          uint32_t Index,
          char const *Str,
          FILE *Stream)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryReadLock();
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::fputs,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkCStringRead(0, Str);
  Checker.checkStreamIsValid(1, Stream);
}

void
TraceThreadListener::
postCfputs(llvm::CallInst const *Call,
           uint32_t Index,
           char const *Str,
           FILE *Stream)
{
  auto const Length = std::strlen(Str);
  recordStreamWriteFromMemory(Stream, MemoryArea(Str, Length));
}


//===------------------------------------------------------------------------===
// getchar
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCgetchar(llvm::CallInst const *Call, uint32_t Index)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::getchar,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStandardStreamIsValid(stdin);
}


//===------------------------------------------------------------------------===
// putchar
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCputchar(llvm::CallInst const *Call, uint32_t Index, int Ch)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::putchar,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStandardStreamIsValid(stdout);
}

void
TraceThreadListener::
postCputchar(llvm::CallInst const *Call, uint32_t Index, int Ch)
{
  char C = Ch;
  recordStreamWrite(stdout, llvm::ArrayRef<char>(&C, 1));
}


//===------------------------------------------------------------------------===
// puts
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCputs(llvm::CallInst const *Call, uint32_t Index, char const *Str)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryReadLock();
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::puts,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkCStringRead(0, Str);
  Checker.checkStandardStreamIsValid(stdout);
}

void
TraceThreadListener::
postCputs(llvm::CallInst const *Call, uint32_t Index, char const *Str)
{
  auto const Length = std::strlen(Str);
  recordStreamWriteFromMemory(stdout, MemoryArea(Str, Length));

  char NL[] = "\n";
  recordStreamWrite(stdout, llvm::ArrayRef<char>(NL, std::strlen(NL)));
}


//===------------------------------------------------------------------------===
// ungetc
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCungetc(llvm::CallInst const *Call, uint32_t Index, int Ch, FILE *Stream)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::ungetc,
                     ProcessListener.getStreams(StreamsLock));
  
  Checker.checkStreamIsValid(1, Stream);
}


//===------------------------------------------------------------------------===
// snprintf
//===------------------------------------------------------------------------===

void
TraceThreadListener::
preCsnprintf(llvm::CallInst const *Call,
             uint32_t Index,
             char *Buffer,
             std::size_t BufSize,
             char const *Str,
             detect_calls::VarArgList<TraceThreadListener> const &Args)
{
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  acquireStreamsLock();
  
  CIOChecker Checker(*this, Index, CStdFunction::snprintf,
                     ProcessListener.getStreams(StreamsLock));
  
  // Check that the format string and varargs are OK.
  Checker.checkPrintFormat(2, Str, Args);
  
  // Check that the Buffer and BufSize are OK.
  auto const BufferAddress = reinterpret_cast<uintptr_t>(Buffer);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(0,
                                                     BufferAddress,
                                                     BufSize,
                                                     MemoryAccess::Write);
}

void
TraceThreadListener::
postCsnprintf(llvm::CallInst const *Call,
              uint32_t Index,
              char *Buffer,
              std::size_t BufSize,
              char const *Str,
              detect_calls::VarArgList<TraceThreadListener> const &Args)
{
  auto Length = std::strlen(Buffer) + 1;
  recordUntypedState(Buffer, Length);
}


//===------------------------------------------------------------------------===
// atof
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatof(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   char const *Str) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Atof;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}


//===------------------------------------------------------------------------===
// atoi
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatoi(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   char const *Str) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Atoi;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}


//===------------------------------------------------------------------------===
// atol
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatol(llvm::CallInst const *Call,
                                   uint32_t Index,
                                   char const *Str) {
  acquireGlobalMemoryReadLock();

  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Atol;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}


//===------------------------------------------------------------------------===
// atoll
//===------------------------------------------------------------------------===

void TraceThreadListener::preCatoll(llvm::CallInst const *Call,
                                    uint32_t Index,
                                    char const *Str) {
  acquireGlobalMemoryReadLock();

  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Atol;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}


//===------------------------------------------------------------------------===
// strtol
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtol(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str,
                                     char **EndPtr,
                                     int Base) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtol);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtol(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      char **EndPtr,
                                      int Base) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtoll
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtoll(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      char **EndPtr,
                                      int Base) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtol);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtoll(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Str,
                                       char **EndPtr,
                                       int Base) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
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
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtoul);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtoul(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Str,
                                       char **EndPtr,
                                       int Base) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtoull
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtoull(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Str,
                                       char **EndPtr,
                                       int Base) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtoul);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtoull(llvm::CallInst const *Call,
                                        uint32_t Index,
                                        char const *Str,
                                        char **EndPtr,
                                        int Base) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtof
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtof(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str,
                                     char **EndPtr) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtod);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtof(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      char **EndPtr) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtod
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtod(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str,
                                     char **EndPtr) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtod);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtod(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      char **EndPtr) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtold
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtold(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      char **EndPtr) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtod);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtold(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Str,
                                       char **EndPtr) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtoimax
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtoimax(llvm::CallInst const *Call,
                                        uint32_t Index,
                                        char const *Str,
                                        char **EndPtr) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtod);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtoimax(llvm::CallInst const *Call,
                                         uint32_t Index,
                                         char const *Str,
                                         char **EndPtr) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
  }
}


//===------------------------------------------------------------------------===
// strtoumax
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrtoumax(llvm::CallInst const *Call,
                                        uint32_t Index,
                                        char const *Str,
                                        char **EndPtr) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strtod);
  Checker.checkCStringRead(0, Str);
  
  if (EndPtr) {
    // Check if we can write to *EndPtr.
    auto const End = reinterpret_cast<uintptr_t>(EndPtr);
    Checker.checkMemoryExistsAndAccessibleForParameter(1,
                                                       End,
                                                       sizeof(char *),
                                                       MemoryAccess::Write);
  }
}

void TraceThreadListener::postCstrtoumax(llvm::CallInst const *Call,
                                         uint32_t Index,
                                         char const *Str,
                                         char **EndPtr) {
  if (EndPtr) {
    auto const EndPtrAddr = reinterpret_cast<uintptr_t>(EndPtr);
    auto const Obj = ActiveFunction->getPointerObject(Call->getArgOperand(0));
    ProcessListener.setInMemoryPointerObject(EndPtrAddr, Obj);

    recordUntypedState(reinterpret_cast<char *>(EndPtr), sizeof(*EndPtr));
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
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address) {
    recordMalloc(Address, Num * Size);

    // Record memset 0 because calloc will clear the memory.
    recordUntypedState(reinterpret_cast<char const *>(Address),
                       Num * Size);
  }

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));

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
      *createRunError<RunErrorType::BadDynamicMemoryAddress>
                     (format_selects::CStdFunction::Free,
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
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address) {
    recordMalloc(Address, Size);
  }

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));
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

  if (Address
      && !ProcessListener.isCurrentDynamicMemoryAllocation(AddressInt)) {
    using namespace seec::runtime_errors;

    handleRunError(
      *createRunError<RunErrorType::BadDynamicMemoryAddress>
                     (format_selects::CStdFunction::Realloc,
                      AddressInt),
      RunErrorSeverity::Fatal,
      Index);
  }
}

void TraceThreadListener::postCrealloc(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       void *Address,
                                       size_t Size) {
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto NewAddress = RTValue->getUIntPtr();

  auto OldAddress = reinterpret_cast<uintptr_t>(Address);

  if (OldAddress) {
    if (Size) {
      if (NewAddress) {
        if (NewAddress == OldAddress) {
          recordRealloc(NewAddress, Size);
        }
        else {
          auto const Alloc =
            ProcessListener.getCurrentDynamicMemoryAllocation(OldAddress);

          // Malloc new address.
          recordMalloc(NewAddress, Size);

          // Record the state that was copied to the new address.
          recordMemmove(OldAddress, NewAddress, Alloc->size());

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

  ActiveFunction->setPointerObject(Call,
                                 ProcessListener.makePointerObject(NewAddress));
}


//===------------------------------------------------------------------------===
// getenv
//===------------------------------------------------------------------------===

void TraceThreadListener::preCgetenv(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Name) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Getenv;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Name);
}

void TraceThreadListener::postCgetenv(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Name) {
  // Get the pointer returned by getenv.
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address) {
    auto Str = reinterpret_cast<char const *>(Address);
    auto Length = std::strlen(Str) + 1; // Include terminating nul byte.

    // Remove knowledge of the existing getenv string at this position (if any).
    removeKnownMemoryRegion(Address);

    // TODO: Delete any existing memory states at this address.

    // Set knowledge of the new string area.
    addKnownMemoryRegion(Address, Length, MemoryPermission::ReadOnly);

    // Set the new string at this address.
    recordUntypedState(Str, Length);
  }

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));
}


//===------------------------------------------------------------------------===
// system
//===------------------------------------------------------------------------===

void TraceThreadListener::preCsystem(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Command) {
  acquireGlobalMemoryReadLock();

  // A NULL Command is valid, so only check if it is non-null.
  if (Command) {
    auto const Fn = seec::runtime_errors::format_selects::CStdFunction::System;
    CStdLibChecker Checker(*this, Index, Fn);
    Checker.checkCStringRead(0, Command);
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
  acquireGlobalMemoryReadLock();
  
  auto const Address = reinterpret_cast<uintptr_t>(Ptr);
  auto const Access = seec::runtime_errors::format_selects::MemoryAccess::Read;
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Memchr;
  CStdLibChecker Checker(*this, Index, Fn);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(0, Address, Num, Access);
}

void TraceThreadListener::postCmemchr(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void const *Ptr,
                                      int Value,
                                      size_t Num)
{
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address)
    ActiveFunction->transferArgPointerObjectToCall(0);
  else
    ActiveFunction->setPointerObject(Call, PointerTarget{});
}


//===------------------------------------------------------------------------===
// memcmp
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemcmp(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     void const *Address1,
                                     void const *Address2,
                                     size_t Size) {
  acquireGlobalMemoryReadLock();

  auto const Addr1Int = reinterpret_cast<uintptr_t>(Address1);
  auto const Addr2Int = reinterpret_cast<uintptr_t>(Address2);
  auto const Access = seec::runtime_errors::format_selects::MemoryAccess::Read;
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Memcmp;
  CStdLibChecker Checker(*this, Index, Fn);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(0, Addr1Int, Size, Access);
  Checker.checkMemoryExistsAndAccessibleForParameter(1, Addr2Int, Size, Access);
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
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Memcpy);

  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  auto const SrcAddr = reinterpret_cast<uintptr_t>(Source);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(1, SrcAddr, Size,
                                                     MemoryAccess::Copy);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(0, DestAddr, Size,
                                                     MemoryAccess::Write);
  
  Checker.checkMemoryDoesNotOverlap(MemoryArea(DestAddr, Size),
                                    MemoryArea(SrcAddr, Size));
}

void TraceThreadListener::postCmemcpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      void const *Source,
                                      size_t Size) {
  ActiveFunction->transferArgPointerObjectToCall(0);

  if (MemoryArea(Destination, Size).intersects(MemoryArea(Source, Size))) {
    recordUntypedState(reinterpret_cast<char const *>(Destination), Size);
  }
  else {
    recordMemmove(reinterpret_cast<uintptr_t>(Source),
                  reinterpret_cast<uintptr_t>(Destination),
                  Size);
  }
}


//===------------------------------------------------------------------------===
// memmove
//===------------------------------------------------------------------------===

void TraceThreadListener::preCmemmove(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      void const *Source,
                                      size_t Size) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Memmove);

  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  auto const SrcAddr = reinterpret_cast<uintptr_t>(Source);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(1, SrcAddr, Size,
                                                     MemoryAccess::Read);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(0, DestAddr, Size,
                                                     MemoryAccess::Write);
}

void TraceThreadListener::postCmemmove(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       void *Destination,
                                       void const *Source,
                                       size_t Size) {
  ActiveFunction->transferArgPointerObjectToCall(0);

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
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Memset);

  auto const Addr = reinterpret_cast<uintptr_t>(Destination);
  
  Checker.checkMemoryExistsAndAccessibleForParameter(0, Addr, Size,
                                                     MemoryAccess::Write);
}

void TraceThreadListener::postCmemset(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      void *Destination,
                                      int Value,
                                      size_t Size) {
  ActiveFunction->transferArgPointerObjectToCall(0);

  recordUntypedState(reinterpret_cast<char const *>(Destination), Size);
}


//===------------------------------------------------------------------------===
// strcat
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcat(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Destination,
                                     char const *Source) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strcat);
  
  // Check that Source is a valid C string and find its length.
  auto const SrcLength = Checker.checkCStringRead(1, Source);
  if (!SrcLength)
    return;
  
  // Check that Destination is a valid C string and find its length.
  auto const DestLength = Checker.checkCStringRead(0, Destination);
  if (!DestLength)
    return;

  // Check if it is OK to copy Source to the end of the Destination.
  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  Checker.checkMemoryExistsAndAccessibleForParameter(0,
                                                     DestAddr + DestLength - 1,
                                                     SrcLength,
                                                     MemoryAccess::Write);
}

void TraceThreadListener::postCstrcat(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source) {
  ActiveFunction->transferArgPointerObjectToCall(0);

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
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strchr;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}

void TraceThreadListener::postCstrchr(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      int Character)
{
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address)
    ActiveFunction->transferArgPointerObjectToCall(0);
  else
    ActiveFunction->setPointerObject(Call, PointerTarget{});
}


//===------------------------------------------------------------------------===
// strcmp
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcmp(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str1,
                                     char const *Str2) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strcmp;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str1);
  Checker.checkCStringRead(1, Str2);
}


//===------------------------------------------------------------------------===
// strcoll
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcoll(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strcoll;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str1);
  Checker.checkCStringRead(1, Str2);
}


//===------------------------------------------------------------------------===
// strcpy
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrcpy(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char *Destination,
                                     char const *Source) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strcpy);
  
  // Check that Source is a valid C string and find its length.
  auto const SrcLength = Checker.checkCStringRead(1, Source);
  if (!SrcLength)
    return;

  // Check if it is OK to copy Source to Destination.
  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  Checker.checkMemoryExistsAndAccessibleForParameter(0,
                                                     DestAddr,
                                                     SrcLength,
                                                     MemoryAccess::Write);
}

void TraceThreadListener::postCstrcpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source) {
  ActiveFunction->transferArgPointerObjectToCall(0);

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
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strspn;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str1);
  Checker.checkCStringRead(1, Str2);
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
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  auto Str = reinterpret_cast<char const *>(Address);
  auto Length = std::strlen(Str) + 1; // Include terminating nul byte.
  
  // Remove knowledge of the existing strerror string (if any).
  removeKnownMemoryRegion(Address);
  
  // TODO: Delete any existing memory states at this address.
  
  // Set knowledge of the new string area.
  addKnownMemoryRegion(Address, Length, MemoryPermission::ReadOnly);
  
  // Set the new string at this address.
  recordUntypedState(Str, Length);

  ActiveFunction->setPointerObject(Call,
                                   ProcessListener.makePointerObject(Address));
}


//===------------------------------------------------------------------------===
// strlen
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrlen(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strlen;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}


//===------------------------------------------------------------------------===
// strncat
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrncat(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Size) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strncat);
  
  // Check that Source is readable for at most Size characters.
  auto const SrcLength = Checker.checkLimitedCStringRead(1, Source, Size);
  if (!SrcLength)
    return;
  
  // Check that Destination is a valid C string and find its length.
  auto const DestLength = Checker.checkCStringRead(0, Destination);
  if (!DestLength)
    return;

  // Check if it is OK to copy Source to the end of the Destination.
  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  Checker.checkMemoryExistsAndAccessibleForParameter(0,
                                                     DestAddr + DestLength - 1,
                                                     SrcLength,
                                                     MemoryAccess::Write);
}

void TraceThreadListener::postCstrncat(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char *Destination,
                                       char const *Source,
                                       size_t Size) {
  ActiveFunction->transferArgPointerObjectToCall(0);

  auto const SrcStrNullChar = std::memchr(Source, '\0', Size);
  auto const SrcStrEnd = SrcStrNullChar
                       ? static_cast<char const *>(SrcStrNullChar)
                       : (Source + Size);
  auto const SrcStrLength = (SrcStrEnd - Source) + 1;
  // Memory has been locked since the pre, so strlen should be safe.
  auto const DestStrLength = std::strlen(Destination) + 1;
  auto const UnchangedChars = DestStrLength - SrcStrLength;
  recordUntypedState(Destination + UnchangedChars, SrcStrLength);
}


//===------------------------------------------------------------------------===
// strncmp
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrncmp(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2,
                                      size_t Num) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strncmp;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkLimitedCStringRead(0, Str1, Num);
  Checker.checkLimitedCStringRead(1, Str2, Num);
}


//===------------------------------------------------------------------------===
// strncpy
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrncpy(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Size) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strncpy);
  
  // Check that we can write to the destination.
  auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
  Checker.checkMemoryExistsAndAccessibleForParameter(0,
                                                     DestAddr,
                                                     Size,
                                                     MemoryAccess::Write);
  
  // Check that we can read from the source.
  auto const SrcSize = Checker.checkLimitedCStringRead(1, Source, Size);
  
  // Check that the destination and source do not overlap.
  Checker.checkMemoryDoesNotOverlap(MemoryArea(Destination, Size),
                                    MemoryArea(Source, SrcSize));
}

void TraceThreadListener::postCstrncpy(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char *Destination,
                                       char const *Source,
                                       size_t Size) {
  ActiveFunction->transferArgPointerObjectToCall(0);

  recordUntypedState(Destination, Size);
}


//===------------------------------------------------------------------------===
// strpbrk
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrpbrk(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strpbrk;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str1);
  Checker.checkCStringRead(0, Str2);
}

void TraceThreadListener::postCstrpbrk(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Str1,
                                       char const *Str2)
{
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address)
    ActiveFunction->transferArgPointerObjectToCall(0);
  else
    ActiveFunction->setPointerObject(Call, PointerTarget{});
}


//===------------------------------------------------------------------------===
// strrchr
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrrchr(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str,
                                      int Character) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strrchr;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str);
}

void TraceThreadListener::postCstrrchr(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char const *Str,
                                       int Character)
{
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address)
    ActiveFunction->transferArgPointerObjectToCall(0);
  else
    ActiveFunction->setPointerObject(Call, PointerTarget{});
}


//===------------------------------------------------------------------------===
// strspn
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrspn(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str1,
                                     char const *Str2) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strspn;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str1);
  Checker.checkCStringRead(1, Str2);
}


//===------------------------------------------------------------------------===
// strstr
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrstr(llvm::CallInst const *Call,
                                     uint32_t Index,
                                     char const *Str1,
                                     char const *Str2) {
  acquireGlobalMemoryReadLock();
  
  auto const Fn = seec::runtime_errors::format_selects::CStdFunction::Strstr;
  CStdLibChecker Checker(*this, Index, Fn);
  Checker.checkCStringRead(0, Str1);
  Checker.checkCStringRead(1, Str2);
}

void TraceThreadListener::postCstrstr(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char const *Str1,
                                      char const *Str2)
{
  auto RTValue = getActiveFunction()->getCurrentRuntimeValue(Call);
  assert(RTValue && RTValue->assigned() && "Expected assigned RTValue.");
  auto Address = RTValue->getUIntPtr();

  if (Address)
    ActiveFunction->transferArgPointerObjectToCall(0);
  else
    ActiveFunction->setPointerObject(Call, PointerTarget{});
}


//===------------------------------------------------------------------------===
// strxfrm
//===------------------------------------------------------------------------===

void TraceThreadListener::preCstrxfrm(llvm::CallInst const *Call,
                                      uint32_t Index,
                                      char *Destination,
                                      char const *Source,
                                      size_t Num) {
  using namespace seec::runtime_errors::format_selects;
  
  acquireGlobalMemoryWriteLock();
  
  CStdLibChecker Checker(*this, Index, CStdFunction::Strxfrm);
  
  if (Destination) {
    Checker.checkLimitedCStringRead(1, Source, Num);
    
    auto const DestAddr = reinterpret_cast<uintptr_t>(Destination);
    Checker.checkMemoryExistsAndAccessibleForParameter(0,
                                                       DestAddr,
                                                       Num,
                                                       MemoryAccess::Write);
  }
  else {
    Checker.checkCStringRead(1, Source);
  }
}

void TraceThreadListener::postCstrxfrm(llvm::CallInst const *Call,
                                       uint32_t Index,
                                       char *Destination,
                                       char const *Source,
                                       size_t Num) {
  if (Destination) {
    auto const Length = std::strlen(Destination + 1);
    recordUntypedState(Destination, Length);
  }
}


} // namespace trace (in seec)

} // namespace seec
