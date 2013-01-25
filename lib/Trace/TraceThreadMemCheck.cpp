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


namespace seec {

namespace trace {


using namespace seec::runtime_errors;


//===------------------------------------------------------------------------===
// getContainingMemoryArea()
//===------------------------------------------------------------------------===

seec::util::Maybe<MemoryArea>
getContainingMemoryArea(TraceThreadListener &Listener,
                        uintptr_t Address) {
  auto const &ProcListener = Listener.getProcessListener();

  // Find the memory area containing Address.
  auto MaybeArea = Listener.getContainingMemoryArea(Address);
  if (!MaybeArea.assigned()) {
    auto ThreadID = Listener.getThreadID();
    MaybeArea = ProcListener.getContainingMemoryArea(Address, ThreadID);
  }

  return MaybeArea;
}


//===------------------------------------------------------------------------===
// RuntimeErrorChecker
//===------------------------------------------------------------------------===

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

bool
RuntimeErrorChecker::memoryExists(uintptr_t Address,
                                  std::size_t Size,
                                  format_selects::MemoryAccess Access,
                                  seec::util::Maybe<MemoryArea> const &Area)
{
  if (Area.assigned())
    return true;
  
  Thread.handleRunError(
    *createRunError<RunErrorType::MemoryUnowned>
                   (Access, Address, Size),
    RunErrorSeverity::Fatal,
    Instruction);
  
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
    Thread.handleRunError(
      *createRunError<RunErrorType::MemoryOverflow>
                     (Access,
                      Address,
                      Size,
                      ArgObject{},
                      ContainingArea.address(),
                      ContainingArea.length()),
      RunErrorSeverity::Fatal,
      Instruction);
    return false;
  }

  // If this is a read, check that the memory is initialized.
  if (Access == format_selects::MemoryAccess::Read) {
    auto const &ProcListener = Thread.getProcessListener();
    auto MemoryState = ProcListener.getTraceMemoryStateAccessor();
    
    if (!MemoryState->hasKnownState(Address, Size)) {
      Thread.handleRunError(
        *createRunError<RunErrorType::MemoryUninitialized>
                       (Address, Size),
        RunErrorSeverity::Fatal,
        Instruction);
      
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

seec::util::Maybe<MemoryArea>
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

  return seec::util::Maybe<MemoryArea>();
}

MemoryArea RuntimeErrorChecker::getLimitedCStringInArea(char const *String,
                                                        MemoryArea Area,
                                                        std::size_t Limit)
{
  auto const MaybeStrArea = getCStringInArea(String, Area);
  
  if (MaybeStrArea.assigned()) {
    auto const Length = std::min(MaybeStrArea.get<0>().length(), Limit);
    return MaybeStrArea.get<0>().withLength(Length);
  }
  
  return MemoryArea(String, Limit);
}


//===------------------------------------------------------------------------===
// CStdLibChecker
//===------------------------------------------------------------------------===

bool
CStdLibChecker::memoryExistsForParameter(
                  unsigned Parameter,
                  uintptr_t Address,
                  std::size_t Size,
                  format_selects::MemoryAccess Access,
                  seec::util::Maybe<MemoryArea> const &Area)
{
  if (Area.assigned())
    return true;
  
  Thread.handleRunError(
    *createRunError<RunErrorType::PassPointerToUnowned>
                   (Function, Address, Parameter),
    RunErrorSeverity::Fatal,
    Instruction);
  
  return false;
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
    Thread.handleRunError(
      createRunError<RunErrorType::PassPointerToInsufficient>
                    (Function,
                     Parameter,
                     Address,
                     Size,
                     ContainingArea.withStart(Address).length(),
                     ArgObject{},
                     ContainingArea.address(),
                     ContainingArea.length())
        ->addAdditional(
          createRunError<RunErrorType::InfoCStdFunctionParameter>
                        (Function, Parameter)),
      RunErrorSeverity::Fatal,
      Instruction);
    
    return false;
  }

  // If this is a read, check that the memory is initialized.
  if (Access == format_selects::MemoryAccess::Read) {
    auto const &ProcListener = Thread.getProcessListener();
    auto MemoryState = ProcListener.getTraceMemoryStateAccessor();
    
    if (!MemoryState->hasKnownState(Address, Size)) {
      Thread.handleRunError(
        *createRunError<RunErrorType::PassPointerToUninitialized>
                       (Function, Address, Parameter),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
  }
  
  return true;
}

bool CStdLibChecker::checkMemoryExistsAndAccessibleForParameter(
                        unsigned Parameter,
                        uintptr_t Address,
                        std::size_t Size,
                        format_selects::MemoryAccess Access)
{
  auto MaybeArea = getContainingMemoryArea(Thread, Address);

  if (!memoryExistsForParameter(Parameter, Address, Size, Access, MaybeArea))
    return false;
  
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

  Thread.handleRunError(
    *createRunError<RunErrorType::OverlappingSourceDest>
                   (Function,
                    Overlap.start(),
                    Overlap.length()),
    RunErrorSeverity::Warning,
    Instruction);
  
  return false;
}

bool CStdLibChecker::checkCStringIsValid(uintptr_t Address,
                                         unsigned Parameter,
                                         seec::util::Maybe<MemoryArea> Area)
{
  if (Area.assigned())
    return true;
  
  Thread.handleRunError(
    *createRunError<RunErrorType::InvalidCString>
                   (Function,
                    Address,
                    Parameter),
    RunErrorSeverity::Fatal,
    Instruction);

  return false;
}

std::size_t CStdLibChecker::checkCStringRead(unsigned Parameter,
                                             char const *String)
{
  auto StrAddr = reinterpret_cast<uintptr_t>(String);

  // Check if String points to owned memory.
  auto const Area = getContainingMemoryArea(Thread, StrAddr);
  if (!memoryExists(StrAddr, 1, format_selects::MemoryAccess::Read, Area))
    return 0;

  // Check if Str points to a valid C string.
  auto const StrArea = getCStringInArea(String, Area.get<0>());
  if (!checkCStringIsValid(StrAddr, Parameter, StrArea))
    return 0;

  auto const StrLength = StrArea.get<0>().length();

  // Check if the read from Str is OK. We already know that the size of the
  // read is valid, from using getCStringInArea, but this will check if the
  // memory is initialized.
  checkMemoryAccessForParameter(Parameter,
                                StrAddr,
                                StrLength,
                                format_selects::MemoryAccess::Read,
                                StrArea.get<0>());
  
  return StrLength;
}

std::size_t CStdLibChecker::checkLimitedCStringRead(unsigned Parameter,
                                                    char const *String,
                                                    std::size_t Limit)
{
  auto StrAddr = reinterpret_cast<uintptr_t>(String);
  
  // Check if String points to owned memory.
  auto const Area = getContainingMemoryArea(Thread, StrAddr);
  if (!memoryExistsForParameter(Parameter,
                                StrAddr,
                                1,
                                format_selects::MemoryAccess::Read,
                                Area)) {
    return 0;
  }
  
  // Find the C string that String refers to, within Limit.
  auto const StrArea = getLimitedCStringInArea(String, Area.get<0>(), Limit);

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
  auto const ArrayAddress = reinterpret_cast<uintptr_t>(Array);
  
  // Check if Array points to owned memory.
  auto const MaybeArea = getContainingMemoryArea(Thread, ArrayAddress);
  if (!memoryExistsForParameter(Parameter,
                                ArrayAddress,
                                sizeof(char *),
                                format_selects::MemoryAccess::Read,
                                MaybeArea)) {
    return 0;
  }
  
  auto const &Area = MaybeArea.get<0>();
  auto const Size = Area.length();
  auto const Elements = Size / sizeof(char *);
  
  // This should be guaranteed by the success of memoryExitsForParameter().
  assert(Elements > 0);
  
  // Ensure that all of the elements are initialized.
  if (!checkMemoryAccessForParameter(Parameter,
                                     ArrayAddress,
                                     Elements * sizeof(char *),
                                     format_selects::MemoryAccess::Read,
                                     Area)) {
    return 0;
  }
  
  // Now check that each element is a valid C string.
  for (unsigned Element = 0; Element < Elements - 1; ++Element) {
    // TODO: When we modify the runtime error system to allow attaching
    //       information piecemeal, make this indicate the exact element in the
    //       array that is in error.
    if (checkCStringRead(Parameter, Array[Element]) == 0) {
      return 0;
    }
  }
  
  if (Array[Elements - 1] != nullptr) {
    Thread.handleRunError(
      *createRunError<seec::runtime_errors::RunErrorType::NonTerminatedArray>
                     (Function, Parameter),
      RunErrorSeverity::Fatal);
    
    return 0;
  }
  
  return Elements;
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
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierParse>
                       (Function, Parameter, StartIndex),
        RunErrorSeverity::Fatal,
        Instruction);
      return false;
    }
    
    auto const EndIndex = Conversion.End - String;
    
    // Ensure that all the specified flags are allowed for this conversion.
    if (Conversion.JustifyLeft && !Conversion.allowedJustifyLeft()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '-'),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    if (Conversion.SignAlwaysPrint && !Conversion.allowedSignAlwaysPrint()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '+'),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    if (Conversion.SignPrintSpace && !Conversion.allowedSignPrintSpace()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, ' '),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    if (Conversion.AlternativeForm && !Conversion.allowedAlternativeForm()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '#'),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    if (Conversion.PadWithZero && !Conversion.allowedPadWithZero()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierFlag>
                       (Function, Parameter, StartIndex, EndIndex, '0'),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    // If a width was specified, ensure that width is allowed.
    if (Conversion.WidthSpecified && !Conversion.allowedWidth()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierWidthDenied>
                       (Function, Parameter, StartIndex, EndIndex),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    // If a precision was specified, ensure that precision is allowed.
    if (Conversion.PrecisionSpecified && !Conversion.allowedPrecision()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierPrecisionDenied>
                       (Function, Parameter, StartIndex, EndIndex),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    // Ensure that the length modifier (if any) is allowed.
    if (!Conversion.allowedCurrentLength()) {
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierLengthDenied>
                       (Function,
                        Parameter,
                        StartIndex,
                        EndIndex,
                        asCFormatLengthModifier(Conversion.Length)),
        RunErrorSeverity::Fatal,
        Instruction);
      
      return false;
    }
    
    // If width is an argument, check that it is readable.
    if (Conversion.WidthAsArgument) {
      if (NextArg < Args.size()) {
        auto MaybeWidth = Args.getAs<int>(NextArg);
        if (!MaybeWidth.assigned()) {
          Thread.handleRunError(
            *createRunError<RunErrorType::FormatSpecifierWidthArgType>
                           (Function,
                            Parameter,
                            StartIndex,
                            EndIndex,
                            Args.offset() + NextArg),
            RunErrorSeverity::Fatal,
            Instruction);
          
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
          Thread.handleRunError(
            *createRunError<RunErrorType::FormatSpecifierPrecisionArgType>
                           (Function,
                            Parameter,
                            StartIndex,
                            EndIndex,
                            Args.offset() + NextArg),
            RunErrorSeverity::Fatal,
            Instruction);
          
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
      Thread.handleRunError(
        *createRunError<RunErrorType::FormatSpecifierArgType>
                       (Function,
                        Parameter,
                        StartIndex,
                        EndIndex,
                        asCFormatLengthModifier(Conversion.Length),
                        Args.offset() + NextArg),
        RunErrorSeverity::Fatal,
        Instruction);
      
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
        
        if (!checkCStringRead(Args.offset() + NextArg, MaybePointer.get<0>()))
          return false;
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
          format_selects::MemoryAccess::Write);
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
    Thread.handleRunError(
      *createRunError<RunErrorType::VarArgsInsufficient>
                     (Function, NextArg, Args.size()),
      RunErrorSeverity::Fatal,
      Instruction);
    
    return false;
  }
  else if (NextArg < Args.size()) {
    Thread.handleRunError(
      *createRunError<RunErrorType::VarArgsSuperfluous>
                     (Function, NextArg, Args.size()),
      RunErrorSeverity::Warning,
      Instruction);
  }
  
  return true;
}


//===------------------------------------------------------------------------===
// CIOChecker
//===------------------------------------------------------------------------===

bool CIOChecker::checkStreamIsValid(unsigned int Parameter,
                                    FILE *Stream) {
  using namespace seec::runtime_errors;
  
  if (!Streams.streamWillClose(Stream)) {
    Thread.handleRunError(*createRunError<RunErrorType::PassInvalidStream>
                                         (Function, Parameter),
                          seec::trace::RunErrorSeverity::Fatal,
                          Instruction);
    
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
    
    Thread.handleRunError(*createRunError<RunErrorType::UseInvalidStream>
                                         (Function, StdStream),
                          seec::trace::RunErrorSeverity::Fatal,
                          Instruction);
    
    return false;
  }

  return true;
}


} // namespace trace (in seec)

} // namespace seec
