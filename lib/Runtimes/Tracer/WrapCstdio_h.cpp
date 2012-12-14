//===- lib/Runtimes/Tracer/WrapCstdio_h.cpp -------------------------------===//
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

#include "Tracer.hpp"

#include "seec/RuntimeErrors/FormatSelects.hpp"
#include "seec/Runtimes/MangleFunction.h"
#include "seec/Trace/DetectCalls.hpp"
#include "seec/Trace/ScanFormatSpecifiers.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Trace/TraceThreadMemCheck.hpp"
#include "seec/Util/ScopeExit.hpp"

#include "llvm/Support/CallSite.h"

#include <cctype>
#include <cinttypes>
#include <cstdio>


extern "C" {

//===----------------------------------------------------------------------===//
// scanf, fscanf, sscanf
//===----------------------------------------------------------------------===//

/// \brief Attempt to match a sequence of literal characters.
///
bool matchNonConversionCharacters(int &CharactersRead,
                                  FILE *Stream,
                                  char const *Start,
                                  char const *End = nullptr)
{
  while(*Start && (End == nullptr || Start < End)) {
    if (std::isspace(*Start)) {
      // Match all whitespace characters.
      int ReadChar;
      
      while ((ReadChar = std::fgetc(Stream)) != EOF) {
        if (!std::isspace(ReadChar)) {
          std::ungetc(ReadChar, Stream);
          break;
        }
        
        ++CharactersRead;
      }
    }
    else {
      // Match a single character.
      int ReadChar = std::fgetc(Stream);
      if (ReadChar == EOF)
        return false;
      
      if (ReadChar != *Start) {
        std::ungetc(ReadChar, Stream);
        return false;
      }
      
      ++CharactersRead;
    }
    
    ++Start;
  }
  
  return true;
}

/// \brief Attempt to match an integer from a stream.
///
bool parseInt(int &CharactersRead,
              FILE *Stream,
              seec::trace::ScanConversionSpecifier const &Conversion,
              std::uintmax_t &Output)
{
  using namespace seec::trace;
  
  char Buffer[64];
  auto const BufferSize = sizeof(Buffer);
  
  auto Width = Conversion.Width;
  if (Width == 0 || Width > BufferSize - 1)
    Width = BufferSize - 1;
  
  bool Unsigned     = false;
  bool HexPrefixOK  = false;
  
  int Base = 0;
  
  switch (Conversion.Conversion) {
    case ScanConversionSpecifier::Specifier::d:
      Base = 10;
      break;
    case ScanConversionSpecifier::Specifier::i:
      break;
    case ScanConversionSpecifier::Specifier::o:
      Base = 8;
      Unsigned = true;
      break;
    case ScanConversionSpecifier::Specifier::u:
      Base = 10;
      Unsigned = true;
      break;
    case ScanConversionSpecifier::Specifier::x:
      Base = 16;
      Unsigned = true;
      break;
    default:
      llvm_unreachable("invalid conversion specifier for parseInt()");
      return false;
  }
  
  char *BufferPtr = Buffer;
  
  for (; Width != 0; --Width) {
    int ReadChar = std::fgetc(Stream);
    if (ReadChar == EOF) {
      if (std::ferror(Stream))
        return false;
      break;
    }
    
    bool ReadOK = true;
    
    switch (ReadChar) {
      // OK always. For %i, this sets the base to octal, unless it is followed
      // by 'x' or 'X', which will set the base to hexadecimal (we take care
      // of that when encountering 'x' or 'X').
      case '0':
        if (Base == 0) {
          Base = 8;
          HexPrefixOK = true;
        }
        break;
      
      // OK always.
      case '1': [[clang::fallthrough]];
      case '2': [[clang::fallthrough]];
      case '3': [[clang::fallthrough]];
      case '4': [[clang::fallthrough]];
      case '5': [[clang::fallthrough]];
      case '6': [[clang::fallthrough]];
      case '7':
        if (Base == 0)
          Base = 10;
        break;
      
      // OK iff decimal or hexadecimal.
      case '8': [[clang::fallthrough]];
      case '9':
        if (Base == 0)
          Base = 10;
        else if (Base < 10)
          ReadOK = false;
        break;
      
      // OK iff hexadecimal.
      case 'A': [[clang::fallthrough]];
      case 'a': [[clang::fallthrough]];
      case 'B': [[clang::fallthrough]];
      case 'b': [[clang::fallthrough]];
      case 'C': [[clang::fallthrough]];
      case 'c': [[clang::fallthrough]];
      case 'D': [[clang::fallthrough]];
      case 'd': [[clang::fallthrough]];
      case 'E': [[clang::fallthrough]];
      case 'e': [[clang::fallthrough]];
      case 'F': [[clang::fallthrough]];
      case 'f':
        if (Base < 16)
          ReadOK = false;
        break;
      
      // OK as first character.
      case '+': [[clang::fallthrough]];
      case '-':
        if (BufferPtr != Buffer)
          ReadOK = false;
        break;
      
      // OK if part of the prefix "0x" or "0X".
      case 'X': [[clang::fallthrough]];
      case 'x':
        if (HexPrefixOK) {
          Base = 16;
          HexPrefixOK = false;
        }
        else {
          ReadOK = false;
        }
        break;
      
      // Any other character is always invalid.
      default:
        ReadOK = false;
    }
    
    if (ReadOK) {
      *BufferPtr++ = static_cast<char>(ReadChar);
      ++CharactersRead;
    }
    else {
      // Push the character back into the stream.
      std::ungetc(ReadChar, Stream);
      break;
    }
  }
  
  *BufferPtr = '\0';
  
  // Read the integer.
  char *ParseEnd = nullptr;
  
  if (Unsigned)
    Output = std::strtoumax(Buffer, &ParseEnd, Base);
  else
    Output = std::strtoimax(Buffer, &ParseEnd, Base);
  
  // Push unused characters back into the stream.
  if (ParseEnd != BufferPtr) {
    llvm::errs() << "\npushing characters back\n";
    for (--BufferPtr; BufferPtr > ParseEnd; --BufferPtr) {
      std::ungetc(*BufferPtr, Stream);
    }
  }
  
  return (ParseEnd != Buffer);
}

// ParseFloat

// ParseCharacterSet

static int
checkStreamScan(seec::runtime_errors::format_selects::CStdFunction FSFunction,
                unsigned VarArgsStartIndex,
                FILE *Stream,
                char const *Format)
{
  using namespace seec::trace;
  
  auto &ThreadEnv = getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Instruction = ThreadEnv.getInstruction();
  auto InstructionIndex = ThreadEnv.getInstructionIndex();
  auto Call = llvm::CallSite(Instruction);
  assert(Call && "expected call or invoke instruction.");
    
  // Interace with the thread listener's notification system.
  Listener.enterNotification();
  auto DoExit = seec::scopeExit([&](){ Listener.exitPostNotification(); });
  
  // Use a VarArgList to access our arguments.
  detect_calls::VarArgList<TraceThreadListener>
    VarArgs{Listener, Call, VarArgsStartIndex};

  // Lock IO streams and global memory.
  Listener.acquireGlobalMemoryWriteLock();
  auto StreamsAccessor = Listener.getProcessListener().getStreamsAccessor();
  
  // Use a CIOChecker to help check memory.
  CIOChecker Checker{Listener,
                     InstructionIndex,
                     FSFunction,
                     StreamsAccessor.getObject()};
  
  // Check that the stream is valid.
  if (Stream == stdin || Stream == stdout || Stream == stderr) {
    Checker.checkStandardStreamIsValid(Stream);
  }
  else {
    Checker.checkStreamIsValid(0, Stream);
  }
  
  // Check and perform the (f)scanf.
  auto FormatSize = Checker.checkCStringRead(VarArgsStartIndex - 1, Format);
  if (!FormatSize)
    return 0;
  
  int Result = 0;
  int NumCharsRead = 0;
  unsigned NextArg = 0;
  char const *NextChar = Format;
  
  while (true) {
    auto Conversion = ScanConversionSpecifier::readNextFrom(NextChar);
    if (!Conversion.Start) {
      // Attempt to match and consume remaining characters.
      matchNonConversionCharacters(NumCharsRead, Stream, NextChar);
      break;
    }
    
    // Attempt to match and consume [NextChar, Conversion.Start).
    if (!matchNonConversionCharacters(NumCharsRead, Stream, NextChar,
                                      Conversion.Start))
      break;
    
    auto const StartIndex = Conversion.Start - Format;
    
    // Ensure that the conversion specifier was parsed correctly.
    if (!Conversion.End) {
      Listener.handleRunError(
        seec::runtime_errors::createRunError
          <seec::runtime_errors::RunErrorType::FormatSpecifierParse>
          (FSFunction, VarArgsStartIndex - 1, StartIndex),
        RunErrorSeverity::Fatal,
        InstructionIndex);
      return Result;
    }
    
    auto const EndIndex = Conversion.End - Format;
    
    // If assignment was suppressed, ensure that suppressing assignment is OK.
    if (Conversion.SuppressAssignment) {
      if (!Conversion.allowedSuppressAssignment()) {
        Listener.handleRunError(
          seec::runtime_errors::createRunError
          <seec::runtime_errors::RunErrorType::FormatSpecifierSuppressionDenied>
            (FSFunction, VarArgsStartIndex - 1, StartIndex, EndIndex),
          RunErrorSeverity::Fatal,
          InstructionIndex);
        return Result;
      }
    }
    else {
      // Check that the argument type matches the expected type. Don't check
      // that the argument exists here, because some conversion specifiers don't
      // require an argument (i.e. %%), so we check if it exists when needed, in
      // the isArgumentTypeOK() implementation.
      if (!Conversion.isArgumentTypeOK(VarArgs, NextArg)) {
        Listener.handleRunError(
          seec::runtime_errors::createRunError
            <seec::runtime_errors::RunErrorType::FormatSpecifierArgType>
            (FSFunction,
             VarArgsStartIndex - 1,
             StartIndex,
             EndIndex,
             asCFormatLengthModifier(Conversion.Length),
             VarArgs.offset() + NextArg),
          RunErrorSeverity::Fatal,
          InstructionIndex);
        return Result;
      }

      // If the argument type is a pointer, check that the destination is
      // writable. The conversion for strings (and sets) is a special case.
      if (Conversion.Conversion == ScanConversionSpecifier::Specifier::s
          || Conversion.Conversion == ScanConversionSpecifier::Specifier::set) {
        if (NextArg < VarArgs.size()) {
          // Check that the field width is specified.
          if (!Conversion.WidthSpecified) {
            Listener.handleRunError(
              seec::runtime_errors::createRunError
                <seec::runtime_errors::RunErrorType::FormatSpecifierWidthMissing>
                (FSFunction,
                 VarArgsStartIndex - 1,
                 StartIndex,
                 EndIndex),
              RunErrorSeverity::Warning,
              InstructionIndex);
          }
          else {
            // Check that the destination is writable and has sufficient space
            // for the field width specified by the programmer.
            auto MaybeArea = Conversion.getArgumentPointee(VarArgs, NextArg);
            auto const Size = (Conversion.Length == LengthModifier::l)
                            ? (Conversion.Width + 1) * sizeof(wchar_t)
                            : (Conversion.Width + 1) * sizeof(char);
            
            if (!Checker.checkMemoryExistsAndAccessibleForParameter(
                    VarArgs.offset() + NextArg,
                    MaybeArea.get<0>().address(),
                    Size,
                    seec::runtime_errors::format_selects::MemoryAccess::Write))
              return false;
          }
        }
      }
      else {
        auto MaybePointeeArea = Conversion.getArgumentPointee(VarArgs, NextArg);
        if (MaybePointeeArea.assigned()) {
          auto Area = MaybePointeeArea.get<0>();
          Checker.checkMemoryExistsAndAccessibleForParameter(
            VarArgs.offset() + NextArg,
            Area.address(),
            Area.length(),
            seec::runtime_errors::format_selects::MemoryAccess::Write);
        }
      }
    }
    
    bool ConversionSuccessful = true;
    
    switch (Conversion.Conversion) {
      case ScanConversionSpecifier::Specifier::none:
        llvm_unreachable("encountered scan conversion specifier \"none\"");
        break;
      
      case ScanConversionSpecifier::Specifier::percent:
        if (!matchNonConversionCharacters(NumCharsRead,
                                          Stream,
                                          Conversion.End - 1,
                                          Conversion.End)) {
          ConversionSuccessful = false;
        }
        break;
      
      case ScanConversionSpecifier::Specifier::c:
        // Read a single char.
        if (Conversion.Length == LengthModifier::none) {
          if (Conversion.SuppressAssignment || NextArg >= VarArgs.size()) {
            if (std::fscanf(Stream, "%*c") == EOF)
              ConversionSuccessful = false;
          }
          else {
            auto Ptr = VarArgs.getAs<char *>(NextArg).get<0>();
            if (std::fscanf(Stream, "%c", Ptr) == 1)
              ++Result;
            else
              ConversionSuccessful = false;
          }
        }
        else if (Conversion.Length == LengthModifier::l) {
          if (Conversion.SuppressAssignment || NextArg >= VarArgs.size()) {
            if (std::fscanf(Stream, "&*lc") == EOF)
              ConversionSuccessful = false;
          }
          else {
            auto Ptr = VarArgs.getAs<wchar_t *>(NextArg).get<0>();
            if (std::fscanf(Stream, "%lc", Ptr) == 1)
              ++Result;
            else
              ConversionSuccessful = false;
          }
        }
        else {
          ConversionSuccessful = false;
        }
        break;
        
      case ScanConversionSpecifier::Specifier::s:
        // Read string.
        {
          auto Width = Conversion.Width;
          if (Width == 0)
            Width = std::numeric_limits<decltype(Width)>::max();
          
          bool InsufficientMemory = false;
          
          if (Conversion.Length == LengthModifier::l) {
            llvm_unreachable("%ls not yet supported.");
          }
          else {
            auto const Dest = NextArg < VarArgs.size()
                            ? VarArgs.getAs<char *>(NextArg).get<0>()
                            : nullptr;
            
            auto const DestAddr = reinterpret_cast<uintptr_t>(Dest);
            
            auto const Writable
                          = DestAddr
                          ? Checker.getSizeOfWritableAreaStartingAt(DestAddr)
                          : 0;
            
            int MatchedChars = 0;
            int WrittenChars = 0;
            int ReadChar;
            
            for (; Width != 0; --Width) {
              if ((ReadChar = std::fgetc(Stream)) == EOF) {
                if (std::ferror(Stream))
                  ConversionSuccessful = false;
                break;
              }
              
              if (std::isspace(ReadChar)) {
                std::ungetc(ReadChar, Stream);
                break;
              }
              else {
                ++MatchedChars;
                ++NumCharsRead;
                
                if (!Conversion.SuppressAssignment) {
                  // Write character.
                  if (WrittenChars < Writable)
                    Dest[WrittenChars++] = static_cast<char>(ReadChar);
                  else
                    InsufficientMemory = true;
                }
              }
            }
            
            if (ConversionSuccessful && !Conversion.SuppressAssignment) {
              // Attempt to nul-terminate the string.
              if (WrittenChars < Writable) {
                Dest[WrittenChars] = '\0';
                ++Result;
              }
              else
                InsufficientMemory = true;
            }
            
            if (InsufficientMemory) {
              using namespace seec::runtime_errors;
              
              // Raise error for insufficient memory in destination buffer.
              Listener.handleRunError(
                createRunError<RunErrorType::ScanFormattedStringOverflow>
                              (FSFunction,
                               VarArgsStartIndex - 1,
                               StartIndex,
                               EndIndex,
                               asCFormatLengthModifier(Conversion.Length),
                               VarArgs.offset() + NextArg,
                               Writable,
                               MatchedChars),
                seec::trace::RunErrorSeverity::Fatal,
                InstructionIndex);
              return Result;
            }
          }
        }
        break;
      
      case ScanConversionSpecifier::Specifier::set:
        // TODO: Read set.
        break;
      
      case ScanConversionSpecifier::Specifier::u: [[clang::fallthrough]];
      case ScanConversionSpecifier::Specifier::d: [[clang::fallthrough]];
      case ScanConversionSpecifier::Specifier::i: [[clang::fallthrough]];
      case ScanConversionSpecifier::Specifier::o: [[clang::fallthrough]];
      case ScanConversionSpecifier::Specifier::x:
        // Read integer.
        {
          std::uintmax_t ReadInt;
          if (!parseInt(NumCharsRead, Stream, Conversion, ReadInt)) {
            ConversionSuccessful = false;
            break;
          }
          
          if (!Conversion.SuppressAssignment && NextArg < VarArgs.size()) {
            ConversionSuccessful
              = Conversion.assignPointee(VarArgs, NextArg, ReadInt);
            ++Result;
          }
        }
        break;
      
      case ScanConversionSpecifier::Specifier::n:
        if (!Conversion.SuppressAssignment) {
          ConversionSuccessful
            = Conversion.assignPointee(VarArgs, NextArg, NumCharsRead);
        }
        break;
      
      case ScanConversionSpecifier::Specifier::a:
      case ScanConversionSpecifier::Specifier::A:
      case ScanConversionSpecifier::Specifier::e:
      case ScanConversionSpecifier::Specifier::E:
      case ScanConversionSpecifier::Specifier::f:
      case ScanConversionSpecifier::Specifier::F:
      case ScanConversionSpecifier::Specifier::g:
      case ScanConversionSpecifier::Specifier::G:
        // TODO: Read float.
        break;
      
      case ScanConversionSpecifier::Specifier::p:
        // Read pointer.
        if (Conversion.SuppressAssignment || NextArg >= VarArgs.size()) {
          if (std::fscanf(Stream, "%*p") == EOF)
            ConversionSuccessful = false;
        }
        else {
          auto Ptr = VarArgs.getAs<void **>(NextArg).get<0>();
          if (std::fscanf(Stream, "%p", Ptr) == 1)
            ++Result;
          else
            ConversionSuccessful = false;
        }
        break;
    }
    
    if (!ConversionSuccessful)
      break;
    
    // Move to the next argument (unless this conversion specifier doesn't
    // consume an argument).
    if (Conversion.Conversion != ScanConversionSpecifier::Specifier::percent
        && Conversion.SuppressAssignment == false) {
      ++NextArg;
    }
    
    // The next position to search from should be the first character following
    // this conversion specifier.
    NextChar = Conversion.End;
  }
  
  // Ensure that we got exactly the right number of arguments.
  if (NextArg > VarArgs.size()) {
    Listener.handleRunError(createRunError<RunErrorType::VarArgsInsufficient>
                                          (FSFunction,
                                           NextArg,
                                           VarArgs.size()),
                            RunErrorSeverity::Fatal,
                            InstructionIndex);
  }
  else if (NextArg < VarArgs.size()) {
    Listener.handleRunError(createRunError<RunErrorType::VarArgsSuperfluous>
                                          (FSFunction,
                                           NextArg,
                                           VarArgs.size()),
                            RunErrorSeverity::Warning,
                            InstructionIndex);
  }
  
  // Record the produced value.
  Listener.notifyValue(InstructionIndex, Instruction, unsigned(Result));
  
  if (NextChar == Format && (std::feof(Stream) || std::ferror(Stream)))
    return EOF;
  
  return Result;
}

int
SEEC_MANGLE_FUNCTION(scanf)
(char const *Format, ...)
{
  return checkStreamScan
            (seec::runtime_errors::format_selects::CStdFunction::scanf,
             1,
             stdin,
             Format);
}

int
SEEC_MANGLE_FUNCTION(fscanf)
(FILE *Stream, char const *Format, ...)
{
  return checkStreamScan
            (seec::runtime_errors::format_selects::CStdFunction::fscanf,
             2,
             Stream,
             Format);
}

int
SEEC_MANGLE_FUNCTION(sscanf)
(char const *Buffer, char const *Format, ...)
{
  auto &ThreadEnv = seec::trace::getThreadEnvironment();
  auto &Listener = ThreadEnv.getThreadListener();
  auto Call = llvm::CallSite(ThreadEnv.getInstruction());
  assert(Call && "expected call or invoke instruction.");
    
  // TODO: Check and do.
  
  // Do.
  llvm_unreachable("sscanf: not implemented.");
  
  // Record.
  
  // Return.
  return 0;
}

} // extern "C"
