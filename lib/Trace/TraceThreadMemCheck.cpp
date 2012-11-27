#include "seec/Preprocessor/Apply.h"
#include "seec/Util/ConstExprCString.hpp"

#include "TraceThreadMemCheck.hpp"

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
// Conversion specifier reading
//===------------------------------------------------------------------------===

/// Length modifier that precedes a conversion specifier.
///
enum class LengthModifier {
  hh,
  h,
  none,
  l,
  ll,
  j,
  z,
  t,
  L
};

/// \brief Represents a single conversion specifier for a print format.
///
struct PrintConversionSpecifier {
  /// Conversion specifier character.
  ///
  enum class Specifier {
    none, ///< Used when no specifier is found.
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
    ID,
#include "PrintFormatSpecifiers.def"
  };
  
  char const *Start; ///< Pointer to the initial '%' character.
  
  char const *End; ///< Pointer to the first character following the specifier.
  
  Specifier Conversion; ///< The type of this specifier.
  
  LengthModifier Length; ///< The length of the argument.
  
  unsigned long Width; ///< Minimum field width.
  
  unsigned long Precision; ///< Precision of conversion.
  
  bool JustifyLeft : 1;         ///< '-' flag.
  bool SignAlwaysPrint : 1;     ///< '+' flag.
  bool SignPrintSpace : 1;      ///< ' ' flag.
  bool AlternativeForm : 1;     ///< '#' flag.
  bool PadWithZero : 1;         ///< '0' flag.
  bool WidthSpecified : 1;      ///< a width was specified.
  bool WidthAsArgument : 1;     ///< '*' width.
  bool PrecisionSpecified : 1;  ///< a precision was specified.
  bool PrecisionAsArgument : 1; ///< '*' precision.
  
  /// \brief Default constructor.
  ///
  PrintConversionSpecifier()
  : Start(nullptr),
    End(nullptr),
    Conversion(Specifier::none),
    Length(LengthModifier::none),
    Width(0),
    Precision(0),
    JustifyLeft(false),
    SignAlwaysPrint(false),
    SignPrintSpace(false),
    AlternativeForm(false),
    PadWithZero(false),
    WidthSpecified(false),
    WidthAsArgument(false),
    PrecisionSpecified(false),
    PrecisionAsArgument(false)
  {}
  
  /// \name Query properties of the current Conversion.
  /// @{
  
  /// \brief Check if this specifier may have JustifyLeft.
  bool allowedJustifyLeft() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return seec::const_strings::contains(FLAGS, '-');
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if this specifier may have SignAlwaysPrint.
  bool allowedSignAlwaysPrint() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return seec::const_strings::contains(FLAGS, '+');
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if this specifier may have SignPrintSpace.
  bool allowedSignPrintSpace() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return seec::const_strings::contains(FLAGS, ' ');
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if this specifier may have AlternativeForm.
  bool allowedAlternativeForm() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return seec::const_strings::contains(FLAGS, '#');
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if this specifier may have PadWithZero.
  bool allowedPadWithZero() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return seec::const_strings::contains(FLAGS, '0');
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if this specifier may have Width.
  bool allowedWidth() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return WIDTH;
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if this specifier may have Precision.
  bool allowedPrecision() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case Specifier::ID: return PREC;
#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// \brief Check if the current length is allowed for this specifier.
  bool allowedCurrentLength() const {
    // We use the X-Macro to generate a two levels of switching. The outer
    // level matches the conversion, and the inner level checks if the current
    // Length is legal for the conversion.
    
    switch (Conversion) {
      case Specifier::none: return false;

#define SEEC_PP_CHECK_LENGTH(LENGTH, TYPE) \
        case LengthModifier::LENGTH: return true;

#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS)  \
      case Specifier::ID:                                                      \
        switch (Length) {                                                      \
          SEEC_PP_APPLY(SEEC_PP_CHECK_LENGTH, LENS)                            \
          default: return false;                                               \
        }

#include "PrintFormatSpecifiers.def"
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// @}
  
  /// \brief Find and read the first print conversion specified in String.
  ///
  /// If no '%' is found, then the returned PrintConversionSpecifier will
  /// be default-constructed (in particular, its Start value will be nullptr).
  ///
  /// If a '%' is found but no valid conversion specifier is detected, then the
  /// returned PrintConversionSpecifier's End value will be nullptr. However,
  /// all other values will be valid if they were specified (flags, width,
  /// precision, and length).
  ///
  static PrintConversionSpecifier readNextFrom(char const * const String) {
    PrintConversionSpecifier Result;
    
    // Find '%'.
    Result.Start = std::strchr(String, '%');
    if (!Result.Start)
      return Result;
    
    auto Remainder = Result.Start;
    
    // Read flags.
    while (*++Remainder) { // Get the next non-null character.
      switch (*Remainder) {
        case '-':
          Result.JustifyLeft = true;
          continue;
        case '+':
          Result.SignAlwaysPrint = true;
          continue;
        case ' ':
          Result.SignPrintSpace = true;
          continue;
        case '#':
          Result.AlternativeForm = true;
          continue;
        case '0':
          Result.PadWithZero = true;
          continue;
        default:
          break;
      }
      
      break; // Not a flag, so exit the flag-reading loop.
    }
    
    // Read width.
    if (*Remainder == '*') {
      Result.WidthSpecified = true;
      Result.WidthAsArgument = true;
      ++Remainder;
    }
    else if(isdigit(*Remainder)){
      Result.WidthSpecified = true;
      
      // Parse int from string.
      char *ParseEnd;
      Result.Width = std::strtoul(Remainder, &ParseEnd, 10);
      Remainder = ParseEnd;
    }
    
    // Read precision.
    if (*Remainder == '.') {
      Result.PrecisionSpecified = true;
      ++Remainder;
      
      if (*Remainder == '*') {
        Result.PrecisionAsArgument = true;
        ++Remainder;
      }
      else if(isdigit(*Remainder)) {
        // Parse int from string. If it does not exist, then the precision is
        // defined to be zero, which it is what strtoul will return on failure.
        char *ParseEnd;
        Result.Precision = std::strtoul(Remainder, &ParseEnd, 10);
        Remainder = ParseEnd;
      }
    }
    
    // Read length modifier.
    switch (*Remainder) {
      case 'h':
        if (*++Remainder == 'h') {
          Result.Length = LengthModifier::hh;
          ++Remainder;
        }
        else {
          Result.Length = LengthModifier::h;
        }
        break;
      case 'l':
        if (*++Remainder == 'l') {
          Result.Length = LengthModifier::ll;
          ++Remainder;
        }
        else {
          Result.Length = LengthModifier::l;
        }
        break;
      case 'j':
        Result.Length = LengthModifier::j;
        ++Remainder;
        break;
      case 'z':
        Result.Length = LengthModifier::z;
        ++Remainder;
        break;
      case 't':
        Result.Length = LengthModifier::t;
        ++Remainder;
        break;
      case 'L':
        Result.Length = LengthModifier::L;
        ++Remainder;
        break;
      default:
        break;
    }
    
    // Read specifier.
    switch (*Remainder) {
#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS) \
      case CHR: Result.Conversion = Specifier::ID; break;
#include "PrintFormatSpecifiers.def"
      default:
        return Result;
    }
    
    // Set the end of the parsed region.
    Result.End = ++Remainder;
    
    return Result;
  }
};


//===------------------------------------------------------------------------===
// RuntimeErrorChecker
//===------------------------------------------------------------------------===

bool
RuntimeErrorChecker::memoryExists(uintptr_t Address,
                                  std::size_t Size,
                                  format_selects::MemoryAccess Access,
                                  seec::util::Maybe<MemoryArea> const &Area)
{
  if (Area.assigned())
    return true;
  
  Thread.handleRunError(createRunError<RunErrorType::MemoryUnowned>(Access,
                                                                    Address,
                                                                    Size),
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
    Thread.handleRunError(createRunError<RunErrorType::MemoryOverflow>
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
      Thread.handleRunError(createRunError<RunErrorType::MemoryUninitialized>
                                          (Address, Size),
                            RunErrorSeverity::Warning,
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
  
  Thread.handleRunError(createRunError<RunErrorType::PassPointerToUnowned>
                                      (Function,
                                       Address,
                                       Parameter),
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
                createRunError<RunErrorType::PassPointerToUninitialized>
                              (Function, Address, Parameter),
                RunErrorSeverity::Warning,
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

  Thread.handleRunError(createRunError<RunErrorType::OverlappingSourceDest>
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
  
  Thread.handleRunError(createRunError<RunErrorType::InvalidCString>
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
  if (!memoryExists(StrAddr, 1, format_selects::MemoryAccess::Read, Area))
    return 0;
  
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
    
    if (!Conversion.End) {
      llvm::errs() << "\nConversion error!\n";
      break;
    }
    
    if (Conversion.JustifyLeft && !Conversion.allowedJustifyLeft()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nJustifyLeft not allowed!\n";
    }
    
    if (Conversion.SignAlwaysPrint && !Conversion.allowedSignAlwaysPrint()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nSignAlwaysPrint not allowed!\n";
    }
    
    if (Conversion.SignPrintSpace && !Conversion.allowedSignPrintSpace()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nSignPrintSpace not allowed!\n";
    }
    
    if (Conversion.AlternativeForm && !Conversion.allowedAlternativeForm()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nAlternativeForm not allowed!\n";
    }
    
    if (Conversion.PadWithZero && !Conversion.allowedPadWithZero()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nPadWithZero not allowed!\n";
    }
    
    if (Conversion.WidthSpecified && !Conversion.allowedWidth()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nWidth not allowed!\n";
    }
    
    if (Conversion.PrecisionSpecified && !Conversion.allowedPrecision()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nPrecision not allowed!\n";
    }
    
    if (!Conversion.allowedCurrentLength()) {
      // TODO: Create runtime error.
      llvm::errs() << "\nLength is incorrect for this specifier!\n";
    }
    
    // If width is an argument, check that it is readable.
    if (Conversion.WidthAsArgument) {
      if (NextArg >= Args.size()) {
        llvm::errs() << "\nInsufficient args!\n";
        return false;
      }
      
      auto MaybeWidth = Args.getAs<int>(NextArg);
      if (!MaybeWidth.assigned()) {
        llvm::errs() << "\nWidth as argument but arg is not an int!\n";
        return false;
      }
      
      ++NextArg;
    }
    
    // If precision is an argument, check that it is readable.
    if (Conversion.PrecisionAsArgument) {
      if (NextArg >= Args.size()) {
        llvm::errs() << "\nInsufficient args!\n";
        return false;
      }
      
      auto MaybePrecision = Args.getAs<int>(NextArg);
      if (!MaybePrecision.assigned()) {
        llvm::errs() << "\nPrecision as argument but arg is not an int!\n";
        return false;
      }
      
      ++NextArg;
    }
    
    // TODO: Check that the argument type matches the expected type.
    
    if (Conversion.Conversion != PrintConversionSpecifier::Specifier::percent) {
      if (NextArg >= Args.size()) {
        llvm::errs() << "\nInsufficient arguments!\n";
        return false;
      }
      
      ++NextArg;
    }
    
    NextChar = Conversion.End;
  }
  
  if (NextArg < Args.size()) {
    // Too many arguments passed to format.
    llvm::errs() << "\nToo many arguments!\n";
  }
  
  return true;
}


//===------------------------------------------------------------------------===
// CStdLibChecker
//===------------------------------------------------------------------------===

bool CIOChecker::checkStreamIsValid(unsigned int Parameter,
                                    FILE *Stream) {
  if (!Streams.streamWillClose(Stream)) {
    Thread.handleRunError(
      seec::runtime_errors::createRunError
        <seec::runtime_errors::RunErrorType::PassInvalidStream>(Function,
                                                                Parameter),
      seec::trace::RunErrorSeverity::Fatal,
      Instruction);
    
    return false;
  }

  return true;
}


} // namespace trace (in seec)

} // namespace seec
