//===- lib/Trace/TraceThreadMemCheck.cpp ----------------------------------===//
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

#include "seec/Trace/PrintFormatSpecifiers.hpp"
#include "seec/Trace/ScanFormatSpecifiers.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/IR/Instruction.h"
#include <signal.h>


namespace seec {

namespace trace {


using namespace seec::runtime_errors;


//===------------------------------------------------------------------------===
// getContainingMemoryArea()
//===------------------------------------------------------------------------===

seec::Maybe<MemoryArea>
getContainingMemoryArea(TraceThreadListener &Listener, uintptr_t Address) {
  auto const &ProcListener = Listener.getProcessListener();
  return ProcListener.getContainingMemoryArea(Address);
}


//===------------------------------------------------------------------------===
// RuntimeErrorChecker
//===------------------------------------------------------------------------===

void RuntimeErrorChecker::raiseError(runtime_errors::RunError &Err,
                                     RunErrorSeverity const Severity)
{
  for (auto const &Note : PermanentNotes)
    Err.addAdditional(Note->clone());
  for (auto const &Note : TemporaryNotes)
    Err.addAdditional(Note->clone());

  Thread.handleRunError(Err, Severity, Instruction);
}

void RuntimeErrorChecker::addPermanentNote(std::unique_ptr<RunError> Note)
{
  PermanentNotes.emplace_back(std::move(Note));
}

void RuntimeErrorChecker::addTemporaryNote(std::unique_ptr<RunError> Note)
{
  TemporaryNotes.emplace_back(std::move(Note));
}

void RuntimeErrorChecker::clearTemporaryNotes()
{
  TemporaryNotes.clear();
}

std::ptrdiff_t
RuntimeErrorChecker::getSizeOfAreaStartingAt(uintptr_t Address)
{
  auto MaybeArea = getContainingMemoryArea(Thread, Address);
  if (!MaybeArea.assigned())
    return 0;
    
  auto const &Area = MaybeArea.get<0>();
  
  return Area.withStart(Address).length();
}

std::ptrdiff_t
RuntimeErrorChecker::getSizeOfWritableAreaStartingAt(uintptr_t Address)
{
  auto MaybeArea = getContainingMemoryArea(Thread, Address);
  if (!MaybeArea.assigned())
    return 0;
    
  auto const &Area = MaybeArea.get<0>();
  if (Area.getAccess() != seec::MemoryPermission::ReadWrite
      && Area.getAccess() != seec::MemoryPermission::WriteOnly)
    return 0;
  
  return Area.withStart(Address).length();
}

bool RuntimeErrorChecker::checkPointer(PointerTarget const &PtrObj,
                                       uintptr_t const Address)
{
  if (!PtrObj) {
    return true;

#if 0
    raiseError(*createRunError<RunErrorType::PointerObjectNULL>(Address),
               RunErrorSeverity::Fatal);

    return false;
#endif
  }

  auto const &Process = Thread.getProcessListener();
  auto const Time = Process.getRegionTemporalID(PtrObj.getBase());
  if (Time != PtrObj.getTemporalID()) {
    raiseError(*createRunError<RunErrorType::PointerObjectOutdated>
                              (PtrObj.getTemporalID(), Time),
               RunErrorSeverity::Fatal);

    return false;
  }

  auto const PtrArea = getContainingMemoryArea(Thread, PtrObj.getBase());

  if (!PtrArea.assigned()) {
    // This gives the most accurate error message for this case: "dereferencing
    // a pointer whose target object has been deallocated". The temporal IDs
    // are not used in the error messages, so it doesn't matter that we don't
    // have a correct ID for the target object.
    raiseError(*createRunError<RunErrorType::PointerObjectOutdated>
                              (PtrObj.getTemporalID(), PtrObj.getTemporalID()),
               RunErrorSeverity::Fatal);

    return false;
  }

  if (!PtrArea.get<MemoryArea>().contains(Address)) {
    raiseError(*createRunError<RunErrorType::PointerObjectMismatch>
                              (PtrObj.getBase(), Address),
               RunErrorSeverity::Fatal);

    return false;
  }

  return true;
}

bool
RuntimeErrorChecker::memoryExists(uintptr_t Address,
                                  std::size_t Size,
                                  format_selects::MemoryAccess Access,
                                  seec::Maybe<MemoryArea> const &Area)
{
  if (Area.assigned())
    return true;
  
  raiseError(*createRunError<RunErrorType::MemoryUnowned>
                            (Access, Address, Size),
             RunErrorSeverity::Fatal);
  
  return false;
}

bool RuntimeErrorChecker::checkMemoryAccess(uintptr_t Address,
                                            std::size_t Size,
                                            format_selects::MemoryAccess Access,
                                            MemoryArea ContainingArea)
{
  // Check that the owned memory area contains the entire load.
  MemoryArea AccessArea(Address, Size);

  if (!ContainingArea.contains(AccessArea)) {
    raiseError(*createRunError<RunErrorType::MemoryOverflow>
                              (Access,
                                Address,
                                Size,
                                ArgObject{},
                                ContainingArea.address(),
                                ContainingArea.length()),
               RunErrorSeverity::Fatal);

    return false;
  }

  // If this is a read, check that the memory is initialized.
  if (Access == format_selects::MemoryAccess::Read) {
    auto const &ProcListener = Thread.getProcessListener();
    auto MemoryState = ProcListener.getTraceMemoryStateAccessor();
    
    if (!MemoryState->hasKnownState(Address, Size)) {
      raiseError(*createRunError<RunErrorType::MemoryUninitialized>
                                (Address, Size),
                 RunErrorSeverity::Fatal);

      return false;
    }
  }

  return true;
}

bool RuntimeErrorChecker::checkMemoryExistsAndAccessible(
                            uintptr_t Address,
                            std::size_t Size,
                            format_selects::MemoryAccess Access)
{
  auto MaybeArea = getContainingMemoryArea(Thread, Address);

  if (!memoryExists(Address, Size, Access, MaybeArea))
    return false;
  
  return checkMemoryAccess(Address, Size, Access, MaybeArea.get<0>());
}

seec::Maybe<MemoryArea>
RuntimeErrorChecker::getCStringInArea(char const *String, MemoryArea Area)
{
  auto const StrAddress = reinterpret_cast<uintptr_t>(String);
  auto const MaxLength = Area.withStart(StrAddress).length();
  
  for (std::size_t Index = 0; Index < MaxLength; ++Index) {
    if (!String[Index]) {
      // Area includes the terminating NUL byte.
      return MemoryArea(StrAddress, Index + 1);
    }
  }

  return seec::Maybe<MemoryArea>();
}

MemoryArea RuntimeErrorChecker::getLimitedCStringInArea(char const *String,
                                                        MemoryArea Area,
                                                        std::size_t Limit)
{
  auto const MaybeStrArea = getCStringInArea(String, Area);
  
  if (MaybeStrArea.assigned()) {
    auto const AreaLen = MaybeStrArea.get<0>().length();
    auto const Length = std::min(AreaLen, uint64_t(Limit));
    return MaybeStrArea.get<0>().withLength(Length);
  }
  
  return MemoryArea(String, Limit);
}


//===------------------------------------------------------------------------===
// CStdLibChecker
//===------------------------------------------------------------------------===

bool
CStdLibChecker::memoryExistsForParameter(unsigned Parameter,
                                         uintptr_t Address,
                                         std::size_t Size,
                                         format_selects::MemoryAccess Access,
                                         seec::Maybe<MemoryArea> const &Area,
                                         PointerTarget const &PtrObj)
{
  // Check that the pointer is valid to use. Do this before checking that the
  // area exists, because we may be able to raise a more specific error if this
  // pointer does not have an associated object.
  //
  if (Call)
    if (!checkPointer(PtrObj, Address))
      return false;

  // Check that the area exists.
  if (!Area.assigned()) {
    raiseError(*createRunError<RunErrorType::PassPointerToUnowned>
                              (Function, Address, Parameter),
               RunErrorSeverity::Fatal);

    return false;
  }

  return true;
}

bool
CStdLibChecker::checkMemoryAccessForParameter(
                  unsigned Parameter,
                  uintptr_t Address,
                  std::size_t Size,
                  format_selects::MemoryAccess Access,
                  MemoryArea ContainingArea)
{
  // Check that the owned memory area contains the entire load.
  MemoryArea AccessArea(Address, Size);

  if (!ContainingArea.contains(AccessArea)) {
    raiseError(*createRunError<RunErrorType::PassPointerToInsufficient>
                              (Function,
                              Parameter,
                              Address,
                              Size,
                              ContainingArea.withStart(Address).length(),
                              ArgObject{},
                              ContainingArea.address(),
                              ContainingArea.length()),
               RunErrorSeverity::Fatal);

    return false;
  }

  // If this is a read, check that the memory is initialized.
  if (Access == format_selects::MemoryAccess::Read) {
    auto const &ProcListener = Thread.getProcessListener();
    auto MemoryState = ProcListener.getTraceMemoryStateAccessor();

    if (!MemoryState->hasKnownState(Address, Size)) {
      raiseError(*createRunError<RunErrorType::PassPointerToUninitialized>
                                (Function, Address, Parameter),
                 RunErrorSeverity::Fatal);

      return false;
    }
  }

  return true;
}

bool CStdLibChecker::checkCStringIsValid(char const *String,
                                         unsigned Parameter,
                                         seec::Maybe<MemoryArea> Area)
{
  if (Area.assigned()) {
    // Check how much memory is initialized within the detected C String. The
    // terminating null-byte that bounds Area may be uninitialized memory, in
    // which case we would prefer to raise InvalidCString than the
    // PassPointerToUninitialized that would be raised later. Unless the
    // string contains no initialized memory at all, in which case
    // PassPointerToUninitialized is more appropriate.
    
    auto const &ProcListener = Thread.getProcessListener();
    auto MemoryState = ProcListener.getTraceMemoryStateAccessor();
    
    auto const &DArea = Area.get<MemoryArea>();
    auto const InitLength = MemoryState->getLengthOfKnownState(DArea.start(),
                                                               DArea.length());
    
    if (InitLength == 0 || InitLength >= DArea.length()) {
      return true;
    }
  }
  
  auto const Address = reinterpret_cast<uintptr_t>(String);
  raiseError(*createRunError<RunErrorType::InvalidCString>
                            (Function, Address, Parameter),
             RunErrorSeverity::Fatal);

  return false;
}

std::size_t CStdLibChecker::checkCStringRead(unsigned Parameter,
                                             char const *String,
                                             PointerTarget const &PtrObj)
{
  auto const ReadAccess = format_selects::MemoryAccess::Read;
  auto const StrAddr = reinterpret_cast<uintptr_t>(String);

  // Check if String points to owned memory.
  auto const Area = getContainingMemoryArea(Thread, StrAddr);
  if (!memoryExistsForParameter(Parameter, StrAddr, 1, ReadAccess, Area,
                                PtrObj))
  {
    return 0;
  }

  // Check if Str points to a valid C string.
  auto const StrArea = getCStringInArea(String, Area.get<0>());
  if (!checkCStringIsValid(String, Parameter, StrArea))
    return 0;

  auto const StrLength = StrArea.get<0>().length();

  // Check if the read from Str is OK. We already know that the size of the
  // read is valid, from using getCStringInArea, but this will check if the
  // memory is initialized.
  checkMemoryAccessForParameter(Parameter,
                                StrAddr,
                                StrLength,
                                ReadAccess,
                                StrArea.get<0>());

  return StrLength;
}

CStdLibChecker::CStdLibChecker(TraceThreadListener &InThread,
                               InstrIndexInFn const InstrIndex,
                               format_selects::CStdFunction const WithFunction)
: RuntimeErrorChecker(InThread, InstrIndex),
  Function(WithFunction),
  CallerIdx(InThread.FunctionStack.size() - 1),
  Call(llvm::dyn_cast<llvm::CallInst>
                     (InThread.getActiveFunction()
                              ->getFunctionIndex()
                              .getInstruction(InstrIndexInFn{InstrIndex})))
{
  addPermanentNote(createRunError<RunErrorType::InfoCStdFunction>(Function));
}

bool CStdLibChecker::checkMemoryExistsAndAccessibleForParameter(
                        unsigned Parameter,
                        uintptr_t Address,
                        std::size_t Size,
                        format_selects::MemoryAccess Access)
{
  addTemporaryNote(createRunError<RunErrorType::InfoCStdFunctionParameter>
                                 (Function, Parameter));
  auto const ClearNotes = seec::scopeExit([this] () { clearTemporaryNotes(); });

  auto MaybeArea = getContainingMemoryArea(Thread, Address);
  auto const PtrVal = Call->getArgOperand(Parameter);
  auto const PtrObj = Thread.FunctionStack[CallerIdx].getPointerObject(PtrVal);

  if (!memoryExistsForParameter(Parameter, Address, Size, Access, MaybeArea,
                                PtrObj))
  {
    return false;
  }

  return checkMemoryAccessForParameter(Parameter,
                                       Address,
                                       Size,
                                       Access,
                                       MaybeArea.get<0>());
}

bool CStdLibChecker::checkMemoryDoesNotOverlap(MemoryArea Area1,
                                               MemoryArea Area2)
{
  auto const Overlap = Area1.intersection(Area2);
  if (!Overlap.length())
    return true;

  raiseError(*createRunError<RunErrorType::OverlappingSourceDest>
                            (Function, Overlap.start(), Overlap.length()),
             RunErrorSeverity::Fatal);

  return false;
}

std::size_t CStdLibChecker::checkCStringRead(unsigned Parameter,
                                             char const *String)
{
  addTemporaryNote(createRunError<RunErrorType::InfoCStdFunctionParameter>
                                 (Function, Parameter));
  auto const ClearNotes = seec::scopeExit([this] () { clearTemporaryNotes(); });

  auto const PtrVal = Call->getArgOperand(Parameter);
  auto const PtrObj = Thread.FunctionStack[CallerIdx].getPointerObject(PtrVal);
  return checkCStringRead(Parameter, String, PtrObj);
}

std::size_t CStdLibChecker::checkLimitedCStringRead(unsigned Parameter,
                                                    char const *String,
                                                    std::size_t Limit)
{
  addTemporaryNote(createRunError<RunErrorType::InfoCStdFunctionParameter>
                                 (Function, Parameter));
  auto const ClearNotes = seec::scopeExit([this] () { clearTemporaryNotes(); });

  auto const ReadAccess = format_selects::MemoryAccess::Read;
  auto const StrAddr = reinterpret_cast<uintptr_t>(String);

  auto const PtrVal = Call->getArgOperand(Parameter);
  auto const PtrObj = Thread.FunctionStack[CallerIdx].getPointerObject(PtrVal);

  // Check if String points to owned memory.
  auto const Area = getContainingMemoryArea(Thread, StrAddr);
  if (!memoryExistsForParameter(Parameter, StrAddr, 1, ReadAccess, Area,
                                PtrObj))
  {
    return 0;
  }

  // Find the C string that String refers to, within Limit.
  auto const StrArea = getLimitedCStringInArea(String, Area.get<0>(), Limit);

  // Check if String points to a valid C string.
  if (!checkCStringIsValid(String, Parameter, StrArea))
    return 0;

  // Check if the read from Str is OK.
  checkMemoryAccessForParameter(Parameter,
                                StrAddr,
                                StrArea.length(),
                                format_selects::MemoryAccess::Read,
                                StrArea);

  return StrArea.length();
}

std::size_t
CStdLibChecker::checkCStringArray(unsigned Parameter, char const * const *Array)
{
  addTemporaryNote(createRunError<RunErrorType::InfoCStdFunctionParameter>
                                 (Function, Parameter));
  auto const ClearNotes = seec::scopeExit([this] () { clearTemporaryNotes(); });

  auto const ArrayAddress = reinterpret_cast<uintptr_t>(Array);

  auto const PtrVal = Call->getArgOperand(Parameter);
  if (!PtrVal->getType()->isPointerTy()) {
    return 0;
  }

  auto const PtrObj = Thread.FunctionStack[CallerIdx].getPointerObject(PtrVal);

  // Check if Array points to owned memory.
  auto const MaybeArea = getContainingMemoryArea(Thread, ArrayAddress);
  if (!memoryExistsForParameter(Parameter,
                                ArrayAddress,
                                sizeof(char *),
                                format_selects::MemoryAccess::Read,
                                MaybeArea,
                                PtrObj))
  {
    return 0;
  }
  
  auto const &Process = Thread.getProcessListener();
  auto const &Area = MaybeArea.get<0>();
  auto const Size = Area.length();
  auto const MaxElements = Size / sizeof(char *);
  bool IsNullTerminated = false;
  unsigned Element;
  
  for (Element = 0; Element < MaxElements; ++Element) {
    // TODO: Add temporary note for InfoElementOfArray!
    auto const ElementAddress = ArrayAddress + (Element * sizeof(char *));
    if (!checkMemoryAccessForParameter(Parameter,
                                       ElementAddress,
                                       1,
                                       format_selects::MemoryAccess::Read,
                                       Area)) {
      return 0;
    }
    
    if (Array[Element] == nullptr) {
      IsNullTerminated = true;
      break;
    }
    
    auto const PtrObj = Process.getInMemoryPointerObject(ElementAddress);
    if (checkCStringRead(Parameter, Array[Element], PtrObj) == 0)
      return 0;
  }
  
  if (!IsNullTerminated) {
    raiseError(
      *createRunError<seec::runtime_errors::RunErrorType::NonTerminatedArray>
                     (Function, Parameter),
      RunErrorSeverity::Fatal);
    
    return 0;
  }
  
  // Return the number of elements, including the terminating null pointer.
  return Element + 1;
}

bool
CStdLibChecker::
checkPrintFormat(unsigned Parameter,
                 const char *String,
                 detect_calls::VarArgList<TraceThreadListener> const &Args)
{
  auto Size = checkCStringRead(Parameter, String);
  if (!Size)
    return false;
  
  unsigned NextArg = 0;
  const char *NextChar = String;
  
  while (true) {
    auto Conversion = PrintConversionSpecifier::readNextFrom(NextChar);
    if (!Conversion.Start)
      break;
    
    auto const StartIndex = Conversion.Start - String;
    
    // Ensure that the conversion specifier was parsed correctly.
    if (!Conversion.End) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierParse>
                       (Function, Parameter, StartIndex),
        RunErrorSeverity::Fatal);
      return false;
    }
    
    auto const EndIndex = Conversion.End - String;
    
    // Ensure that all the specified flags are allowed for this conversion.
    if (Conversion.JustifyLeft && !Conversion.allowedJustifyLeft()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '-'),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    if (Conversion.SignAlwaysPrint && !Conversion.allowedSignAlwaysPrint()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '+'),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    if (Conversion.SignPrintSpace && !Conversion.allowedSignPrintSpace()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, ' '),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    if (Conversion.AlternativeForm && !Conversion.allowedAlternativeForm()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '#'),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    if (Conversion.PadWithZero && !Conversion.allowedPadWithZero()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '0'),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    // If a width was specified, ensure that width is allowed.
    if (Conversion.WidthSpecified && !Conversion.allowedWidth()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierWidthDenied>
                       (Function, Parameter, StartIndex, EndIndex),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    // If a precision was specified, ensure that precision is allowed.
    if (Conversion.PrecisionSpecified && !Conversion.allowedPrecision()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierPrecisionDenied>
                       (Function, Parameter, StartIndex, EndIndex),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    // Ensure that the length modifier (if any) is allowed.
    if (!Conversion.allowedCurrentLength()) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierLengthDenied>
                       (Function,
                        Parameter,
                        StartIndex,
                        EndIndex,
                        asCFormatLengthModifier(Conversion.Length)),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    // If width is an argument, check that it is readable.
    if (Conversion.WidthAsArgument) {
      if (NextArg < Args.size()) {
        auto MaybeWidth = Args.getAs<int>(NextArg);
        if (!MaybeWidth.assigned()) {
          raiseError(
            *createRunError<RunErrorType::FormatSpecifierWidthArgType>
                           (Function,
                            Parameter,
                            StartIndex,
                            EndIndex,
                            Args.offset() + NextArg),
            RunErrorSeverity::Fatal);
          
          return false;
        }
      }
      
      ++NextArg;
    }
    
    // If precision is an argument, check that it is readable.
    if (Conversion.PrecisionAsArgument) {
      if (NextArg < Args.size()) {
        auto MaybePrecision = Args.getAs<int>(NextArg);
        if (!MaybePrecision.assigned()) {
          raiseError(
            *createRunError<RunErrorType::FormatSpecifierPrecisionArgType>
                           (Function,
                            Parameter,
                            StartIndex,
                            EndIndex,
                            Args.offset() + NextArg),
            RunErrorSeverity::Fatal);
          
          return false;
        }
      }
      
      ++NextArg;
    }
    
    // Check that the argument type matches the expected type. Don't check that
    // the argument exists here, because some conversion specifiers don't
    // require an argument (i.e. %%), so we check if it exists when needed, in
    // the isArgumentTypeOK() implementation.
    if (!Conversion.isArgumentTypeOK(Args, NextArg)) {
      raiseError(
        *createRunError<RunErrorType::FormatSpecifierArgType>
                       (Function,
                        Parameter,
                        StartIndex,
                        EndIndex,
                        asCFormatLengthModifier(Conversion.Length),
                        Args.offset() + NextArg),
        RunErrorSeverity::Fatal);
      
      return false;
    }
    
    // If the argument type is a pointer, check that the destination is
    // readable. The conversion for strings is a special case.
    if (Conversion.Conversion == PrintConversionSpecifier::Specifier::s) {
      // Check string is readable.
      if (NextArg < Args.size()) {
        auto MaybePointer = Args.getAs<char const *>(NextArg);
        if (!MaybePointer.assigned()) {
          llvm_unreachable("unassigned char const * for string conversion.");
          return false;
        }
        
        auto const TheParam = Args.offset() + NextArg;
        auto const TheString = MaybePointer.get<0>();
        
        if (Conversion.WidthSpecified) {
          if (!checkLimitedCStringRead(TheParam, TheString, Conversion.Width))
            return false;
        }
        else {
          if (!checkCStringRead(TheParam, TheString))
            return false;
        }
      }
    }
    else {
      auto MaybePointeeArea = Conversion.getArgumentPointee(Args, NextArg);
      if (MaybePointeeArea.assigned()) {
        auto Area = MaybePointeeArea.get<0>();
        checkMemoryExistsAndAccessibleForParameter(
          Args.offset() + NextArg,
          Area.address(),
          Area.length(),
          format_selects::MemoryAccess::Read);
      }
    }
    
    // Move to the next argument (unless this conversion specifier doesn't
    // consume an argument, which only occurs for %%).
    if (Conversion.Conversion != PrintConversionSpecifier::Specifier::percent) {
      ++NextArg;
    }
    
    // The next position to search from should be the first character following
    // this conversion specifier.
    NextChar = Conversion.End;
  }
  
  // Ensure that we got exactly the right number of arguments.
  if (NextArg > Args.size()) {
    raiseError(
      *createRunError<RunErrorType::VarArgsInsufficient>
                     (Function, NextArg, Args.size()),
      RunErrorSeverity::Fatal);
    
    return false;
  }
  else if (NextArg < Args.size()) {
    raiseError(
      *createRunError<RunErrorType::VarArgsSuperfluous>
                     (Function, NextArg, Args.size()),
      RunErrorSeverity::Warning);
  }
  
  return true;
}


//===------------------------------------------------------------------------===
// CIOChecker
//===------------------------------------------------------------------------===

bool CIOChecker::checkStreamIsValid(unsigned int Parameter,
                                    FILE *Stream)
{
  using namespace seec::runtime_errors;

  addTemporaryNote(createRunError<RunErrorType::InfoCStdFunctionParameter>
                                 (Function, Parameter));
  auto const ClearNotes = seec::scopeExit([this] () { clearTemporaryNotes(); });

  auto const FILEAddr = reinterpret_cast<uintptr_t>(Stream);
  auto const PtrVal = Call->getArgOperand(Parameter);
  auto const PtrObj = Thread.FunctionStack[CallerIdx].getPointerObject(PtrVal);
  auto const Time = Thread.getProcessListener().getRegionTemporalID(FILEAddr);

  if (PtrObj.getTemporalID() != Time) {
    raiseError(*createRunError<RunErrorType::PointerObjectOutdated>
                              (PtrObj.getTemporalID(), Time),
               RunErrorSeverity::Fatal);

    return false;
  }

  if (!Streams.streamWillClose(Stream)) {
    raiseError(*createRunError<RunErrorType::PassInvalidStream>
                              (Function, Parameter),
               seec::trace::RunErrorSeverity::Fatal);
    
    return false;
  }

  return true;
}


bool CIOChecker::checkStandardStreamIsValid(FILE *Stream) {
  using namespace seec::runtime_errors;
  using namespace seec::runtime_errors::format_selects;
  
  if (!Streams.streamWillClose(Stream)) {
    CStdStream StdStream = CStdStream::Unknown;
    
    if (Stream == stdout)
      StdStream = CStdStream::Out;
    else if (Stream == stdin)
      StdStream = CStdStream::In;
    else if (Stream == stderr)
      StdStream = CStdStream::Err;
    else
      llvm_unreachable("non-standard stream!");
    
    raiseError(*createRunError<RunErrorType::UseInvalidStream>
                              (Function, StdStream),
               seec::trace::RunErrorSeverity::Fatal);
    
    return false;
  }

  return true;
}


//===------------------------------------------------------------------------===
// DIRChecker
//===------------------------------------------------------------------------===

bool DIRChecker::checkDIRIsValid(unsigned int const Parameter,
                                 void const * const TheDIR)
{
  using namespace seec::runtime_errors;
  
  if (!Dirs.DIRWillClose(TheDIR)) {
    Thread.handleRunError(*createRunError<RunErrorType::PassInvalidDIR>
                                         (Function, Parameter),
                          seec::trace::RunErrorSeverity::Fatal,
                          InstructionIndex);
    
    return false;
  }

  return true;
}


} // namespace trace (in seec)

} // namespace seec
