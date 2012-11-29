#ifndef SEEC_TRACE_SCANFORMATSPECIFIERS_HPP
#define SEEC_TRACE_SCANFORMATSPECIFIERS_HPP

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

#include <string>
#include <type_traits>

namespace seec {

namespace trace {


/// \brief Represents a single conversion specifier for a scan format.
///
struct ScanConversionSpecifier {
  /// Conversion specifier character.
  ///
  enum class Specifier {
    none, ///< Used when no specifier is found.
#define SEEC_SCAN_FORMAT_SPECIFIER(ID, CHR, SUPPRESS, LENS) \
    ID,
#include "ScanFormatSpecifiers.def"
  };
  
  
  char const *Start; ///< Pointer to the initial '%' character.
  
  char const *End; ///< Pointer to the first character following the specifier.
  
  LengthModifier Length; ///< The length of the argument.
  
  Specifier Conversion; ///< The type of this specifier.
  
  unsigned long Width; ///< Maximum field width.
  
  bool WidthSpecified; ///< A width was specified.
  
  bool SuppressAssignment; ///< Assignment suppression specified (by '*').
  
  bool SetNegation; ///< The set started with a ^ character.
  
  std::string SetCharacters; ///< All characters in set.
  
      
  /// \brief Default constructor.
  ///
  ScanConversionSpecifier()
  : Start(nullptr),
    End(nullptr),
    Length(LengthModifier::none),
    Conversion(Specifier::none),
    Width(0),
    WidthSpecified(false),
    SuppressAssignment(false),
    SetNegation(false),
    SetCharacters()
  {}
  
  /// \name Query properties of the current Conversion.
  /// @{
  
  /// \brief Check if this specifier may have SuppressAssignment.
  bool allowedSuppressAssignment() const {
    switch (Conversion) {
      case Specifier::none: return false;
      
#define SEEC_SCAN_FORMAT_SPECIFIER(ID, CHR, SUPPRESS, LENS) \
      case Specifier::ID: return SUPPRESS;
#include "ScanFormatSpecifiers.def"
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

#define SEEC_SCAN_FORMAT_SPECIFIER(ID, CHR, SUPPRESS, LENS)                    \
      case Specifier::ID:                                                      \
        switch (Length) {                                                      \
          SEEC_PP_APPLY(SEEC_PP_CHECK_LENGTH, LENS)                            \
          default: return false;                                               \
        }

#include "ScanFormatSpecifiers.def"
#undef SEEC_PP_CHECK_LENGTH
    }
    
    llvm_unreachable("illegal conversion specifier");
    return false;
  }
  
private:
  /// \brief For pointer types return the MemoryArea of the pointee.
  ///
  template<typename T>
  typename std::enable_if<std::is_pointer<T>::value,
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
  typename std::enable_if<!std::is_pointer<T>::value,
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

#define SEEC_SCAN_FORMAT_SPECIFIER(ID, CHR, SUPPRESS, LENS)                    \
      case Specifier::ID:                                                      \
        switch (Length) {                                                      \
          SEEC_PP_APPLY(SEEC_PP_CHECK_LENGTH, LENS)                            \
          default: return seec::util::Maybe<MemoryArea>();                     \
        }

#include "ScanFormatSpecifiers.def"
#undef SEEC_PP_CHECK_LENGTH
    }
    
    llvm_unreachable("illegal conversion specifier");
    return seec::util::Maybe<MemoryArea>();
  }
  
  /// \brief Find and read the first scan conversion specified in String.
  ///
  /// If no '%' is found, then the returned ScanConversionSpecifier will
  /// be default-constructed (in particular, its Start value will be nullptr).
  ///
  /// If a '%' is found but no valid conversion specifier is detected, then the
  /// returned ScanConversionSpecifier's End value will be nullptr.
  ///
  static ScanConversionSpecifier readNextFrom(char const * const String);
};


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_SCANFORMATSPECIFIERS_HPP
