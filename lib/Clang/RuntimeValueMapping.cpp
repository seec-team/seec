//===- lib/Clang/RuntimeValueMapping.cpp ----------------------------------===//
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

#include "seec/Clang/RuntimeValueMapping.hpp"
#include "seec/Trace/RuntimeValue.hpp"

#include "clang/AST/CanonicalType.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <string>

namespace seec {

namespace seec_clang {


/// \name ConvertBuiltinToString
/// @{

template<typename T>
struct ConvertBuiltinToString {
  static std::string impl(llvm::Instruction const *Instruction,
                          seec::trace::RuntimeValue const &Value) {
    std::string RetStr;
    
    {
      llvm::raw_string_ostream Stream(RetStr);
      Stream << seec::trace::getAs<T>(Value, Instruction->getType());
    }
    
    return RetStr;
  }
};

template<>
struct ConvertBuiltinToString<long double> {
  static std::string impl(llvm::Instruction const *Instruction,
                          seec::trace::RuntimeValue const &Value) {
    auto const LDValue = Value.getLongDouble();
    auto const StringLength = snprintf(nullptr, 0, "%Lf", LDValue);
    if (StringLength < 0)
      return std::string("<long double: snprintf failed>");
    
    char Buffer[StringLength + 1];
    if (snprintf(Buffer, StringLength + 1, "%Lf", LDValue) < 0)
      return std::string("<long double: snprintf failed>");
    
    return std::string(Buffer);
  }
};

template<>
struct ConvertBuiltinToString<void> {
  static std::string impl(llvm::Instruction const *Instruction,
                          seec::trace::RuntimeValue const &Value) {
    return std::string("void");
  }
};

/// @} (ConvertBuiltinToString)


/// \name toString()
/// @{

/// \brief 
static
std::string
toString(clang::BuiltinType const *Type,
         llvm::Instruction const *Instruction,
         seec::trace::RuntimeValue const &Value) {
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN(KIND, HOST_TYPE)                              \
    case clang::BuiltinType::KIND:                                        \
      return ConvertBuiltinToString<HOST_TYPE>::impl(Instruction, Value);

#define SEEC_UNHANDLED_BUILTIN(KIND)                         \
    case clang::BuiltinType::KIND:                           \
      return std::string("unhandled builtin \"" #KIND "\""); 

    // Builtin types
    SEEC_HANDLE_BUILTIN(Void, void)

    // Unsigned types
    SEEC_HANDLE_BUILTIN(Bool, bool)
    SEEC_HANDLE_BUILTIN(Char_U, char)
    SEEC_HANDLE_BUILTIN(UChar, unsigned char)
    SEEC_HANDLE_BUILTIN(WChar_U, wchar_t)
    SEEC_HANDLE_BUILTIN(Char16, char16_t)
    SEEC_HANDLE_BUILTIN(Char32, char32_t)
    SEEC_HANDLE_BUILTIN(UShort, unsigned short)
    SEEC_HANDLE_BUILTIN(UInt, unsigned int)
    SEEC_HANDLE_BUILTIN(ULong, unsigned long)
    SEEC_HANDLE_BUILTIN(ULongLong, unsigned long long)
    SEEC_UNHANDLED_BUILTIN(UInt128)

    // Signed types
    SEEC_HANDLE_BUILTIN(Char_S, char)
    SEEC_HANDLE_BUILTIN(SChar, signed char)
    SEEC_HANDLE_BUILTIN(WChar_S, wchar_t)
    SEEC_HANDLE_BUILTIN(Short, short)
    SEEC_HANDLE_BUILTIN(Int, int)
    SEEC_HANDLE_BUILTIN(Long, long)
    SEEC_HANDLE_BUILTIN(LongLong, long long)
    SEEC_UNHANDLED_BUILTIN(Int128)

    // Floating point types
    SEEC_UNHANDLED_BUILTIN(Half)
    SEEC_HANDLE_BUILTIN(Float, float)
    SEEC_HANDLE_BUILTIN(Double, double)
    SEEC_HANDLE_BUILTIN(LongDouble, long double)

    // Language-specific types
    SEEC_UNHANDLED_BUILTIN(NullPtr)
    SEEC_UNHANDLED_BUILTIN(ObjCId)
    SEEC_UNHANDLED_BUILTIN(ObjCClass)
    SEEC_UNHANDLED_BUILTIN(ObjCSel)
    SEEC_UNHANDLED_BUILTIN(OCLImage1d)
    SEEC_UNHANDLED_BUILTIN(OCLImage1dArray)
    SEEC_UNHANDLED_BUILTIN(OCLImage1dBuffer)
    SEEC_UNHANDLED_BUILTIN(OCLImage2d)
    SEEC_UNHANDLED_BUILTIN(OCLImage2dArray)
    SEEC_UNHANDLED_BUILTIN(OCLImage3d)
    SEEC_UNHANDLED_BUILTIN(OCLEvent)
    SEEC_UNHANDLED_BUILTIN(Dependent)
    SEEC_UNHANDLED_BUILTIN(Overload)
    SEEC_UNHANDLED_BUILTIN(BoundMember)
    SEEC_UNHANDLED_BUILTIN(PseudoObject)
    SEEC_UNHANDLED_BUILTIN(UnknownAny)
    SEEC_UNHANDLED_BUILTIN(BuiltinFn)
    SEEC_UNHANDLED_BUILTIN(ARCUnbridgedCast)

#undef SEEC_HANDLE_BUILTIN
#undef SEEC_UNHANDLED_BUILTIN
  }
  
  return std::string("unmatched builtin kind");
}

static
std::string
toString(clang::Type const *Type,
         llvm::Instruction const *Instruction,
         seec::trace::RuntimeValue const &Value) {
  switch (Type->getTypeClass()) {
    case clang::Type::Builtin:
      return toString(llvm::cast<clang::BuiltinType>(Type), Instruction, Value);
    
    case clang::Type::Pointer:
      return ConvertBuiltinToString<void const *>::impl(Instruction, Value);
    
    default:
      break;
  }
  
  // TODO: describe type.
  return std::string("unhandled type");
}

// Documented in seec/Clang/RuntimeValueMapping.hpp
std::string
toString(clang::Stmt const *Statement,
         llvm::Instruction const *Instruction,
         seec::trace::RuntimeValue const &Value) {
  if (!Value.assigned())
    return std::string("uninitialized");
  
  assert(Statement && Instruction);
  
  auto Expression = llvm::dyn_cast<clang::Expr>(Statement);
  if (!Expression)
    return std::string("not an expression");
  
  auto ExprType = Expression->getType();
  auto CanonExprType = ExprType->getCanonicalTypeUnqualified()->getTypePtr();
  
  std::string RetStr("(");
  RetStr += ExprType.getAsString();
  RetStr += ")";
  RetStr += toString(CanonExprType, Instruction, Value);
  
  return RetStr;
}

/// @} (toString())


} // namespace seec_clang (in seec)

} // namespace seec
