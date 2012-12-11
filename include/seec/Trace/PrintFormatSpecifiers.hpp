//===- lib/Trace/PrintFormatSpecifiers.hpp --------------------------------===//
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

#ifndef SEEC_TRACE_PRINTFORMATSPECIFIERS_HPP
#define SEEC_TRACE_PRINTFORMATSPECIFIERS_HPP

#include "FormatSpecifiers.hpp"

#include "seec/DSA/MemoryArea.hpp"
#include "seec/Preprocessor/Apply.h"
#include "seec/RuntimeErrors/FormatSelects.hpp"
#include "seec/Trace/DetectCalls.hpp"
#include "seec/Trace/TraceThreadListener.hpp"
#include "seec/Util/ConstExprCString.hpp"
#include "seec/Util/DefaultArgPromotion.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Support/ErrorHandling.h"

#include <type_traits>

namespace seec {

namespace trace {


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
#undef SEEC_PP_CHECK_LENGTH
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
  /// @}
  
private:
  /// \brief Check if the argument type matches the given type.
  ///
  template<typename T>
  typename std::enable_if<!std::is_void<T>::value, bool>::type
  checkArgumentType(detect_calls::VarArgList<TraceThreadListener> const &Args,
                    unsigned ArgIndex) const {
    if (ArgIndex < Args.size()) {
      typedef typename default_arg_promotion_of<T>::type PromotedT;
      auto MaybeArg = Args.getAs<PromotedT>(ArgIndex);
      return MaybeArg.assigned();
    }
    
    return false;
  }
  
  /// \brief Always accept conversions that require a void argument.
  ///
  /// This is used to represent conversions that take no argument, e.g. %%.
  ///
  template<typename T>
  typename std::enable_if<std::is_void<T>::value, bool>::type
  checkArgumentType(detect_calls::VarArgList<TraceThreadListener> const &Args,
                    unsigned ArgIndex) const {
    return true;
  }
  
public:
  /// \brief Check if the argument type matches the required type.
  ///
  bool
  isArgumentTypeOK(detect_calls::VarArgList<TraceThreadListener> const &Args,
                   unsigned ArgIndex) const {
    // We use the X-Macro to generate a two levels of switching. The outer
    // level matches the conversion, and the inner level gets the appropriate
    // type given the current Length. If no appropriate type exists, then we
    // return false.
    
    switch (Conversion) {
      case Specifier::none: return false;

#define SEEC_PP_CHECK_LENGTH(LENGTH, TYPE)                                     \
        case LengthModifier::LENGTH:                                           \
          return checkArgumentType<TYPE>(Args, ArgIndex);

#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS)  \
      case Specifier::ID:                                                      \
        switch (Length) {                                                      \
          SEEC_PP_APPLY(SEEC_PP_CHECK_LENGTH, LENS)                            \
          default: return false;                                               \
        }

#include "PrintFormatSpecifiers.def"
#undef SEEC_PP_CHECK_LENGTH
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
private:
  /// \brief For non-void pointer types return the MemoryArea of the pointee.
  ///
  template<typename T>
  typename std::enable_if
           <std::is_pointer<T>::value
            && !std::is_void<typename std::remove_pointer<T>::type>::value,
            seec::util::Maybe<MemoryArea>>::type
  getArgumentPointee(detect_calls::VarArgList<TraceThreadListener> const &Args,
                     unsigned ArgIndex) const {
    if (ArgIndex < Args.size()) {
      auto MaybeArg = Args.getAs<T>(ArgIndex);
      if (MaybeArg.assigned()) {
        auto const Ptr = MaybeArg.template get<0>();
        return MemoryArea(Ptr, sizeof(*Ptr));
      }
    }
    
    return seec::util::Maybe<MemoryArea>();
  }
  
  /// \brief For non-pointer types return an uninitialized Maybe.
  template<typename T>
  typename std::enable_if
           <!std::is_pointer<T>::value
            || std::is_void<typename std::remove_pointer<T>::type>::value,
            seec::util::Maybe<MemoryArea>>::type
  getArgumentPointee(detect_calls::VarArgList<TraceThreadListener> const &Args,
                     unsigned ArgIndex) const {
    return seec::util::Maybe<MemoryArea>();
  }

public:
  /// \brief Get the address and size of the pointee of a pointer argument.
  ///
  seec::util::Maybe<MemoryArea>
  getArgumentPointee(detect_calls::VarArgList<TraceThreadListener> const &Args,
                     unsigned ArgIndex) const {
    // We use the X-Macro to generate a two levels of switching. The outer
    // level matches the conversion, and the inner level gets the appropriate
    // type given the current Length. If no appropriate type exists, then we
    // return an unassigned Maybe.
    
    switch (Conversion) {
      case Specifier::none: return seec::util::Maybe<MemoryArea>();

#define SEEC_PP_CHECK_LENGTH(LENGTH, TYPE)                                     \
        case LengthModifier::LENGTH:                                           \
          return getArgumentPointee<TYPE>(Args, ArgIndex);

#define SEEC_PRINT_FORMAT_SPECIFIER(ID, CHR, FLAGS, WIDTH, PREC, DPREC, LENS)  \
      case Specifier::ID:                                                      \
        switch (Length) {                                                      \
          SEEC_PP_APPLY(SEEC_PP_CHECK_LENGTH, LENS)                            \
          default: return seec::util::Maybe<MemoryArea>();                     \
        }

#include "PrintFormatSpecifiers.def"
#undef SEEC_PP_CHECK_LENGTH
    }
    
    llvm_unreachable("illegal conversion specifier");
    return seec::util::Maybe<MemoryArea>();
  }
  
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
  static PrintConversionSpecifier readNextFrom(char const * const String);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_PRINTFORMATSPECIFIERS_HPP
