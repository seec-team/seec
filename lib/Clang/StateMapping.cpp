//===- lib/Clang/StateMapping.cpp -----------------------------------------===//
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

#include "seec/Clang/StateMapping.hpp"

#include "clang/AST/CanonicalType.h"
#include "clang/AST/Decl.h"

#include "llvm/Support/raw_ostream.h"

#include <string>

namespace seec {

namespace seec_clang {


/// \name ConvertBuiltinToString
/// @{

template<typename T>
struct ConvertBuiltinToString {
  static std::string impl(llvm::ArrayRef<char> State) {
    if (State.size() == sizeof(T)) {
      std::string RetStr;
      
      {
        llvm::raw_string_ostream Stream(RetStr);
        Stream << *reinterpret_cast<T const *>(State.data());
      } // Leaving scope will ensure the stream is flushed into RetStr.
      
      return RetStr;
    }
    
    return std::string("size mismatch");
  }
};

template<>
struct ConvertBuiltinToString<long double> {
  static std::string impl(llvm::ArrayRef<char> State) {
    if (State.size() != sizeof(long double))
      return std::string("<long double: size mismatch>");
    
    auto const Value = *reinterpret_cast<long double const *>(State.data());
    auto const StringLength = snprintf(nullptr, 0, "%Lf", Value);
    if (StringLength < 0)
      return std::string("<long double: snprintf failed>");
    
    char Buffer[StringLength + 1];
    if (snprintf(Buffer, StringLength + 1, "%Lf", Value) < 0)
      return std::string("<long double: snprintf failed>");
    
    return std::string(Buffer);
  }
};

template<>
struct ConvertBuiltinToString<void> {
  static std::string impl(llvm::ArrayRef<char> State) {
    return std::string("void");
  }
};

/// @} (ConvertBuiltinToString)


/// \name toString()
/// @{

std::string
toString(clang::Type const *Type, llvm::ArrayRef<char> State);

/// \brief Get a string representation of State interpreted as the given
///        builtin type.
std::string
toString(clang::BuiltinType const *Type, llvm::ArrayRef<char> State) {
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN(KIND, HOST_TYPE)                 \
    case clang::BuiltinType::KIND:                           \
      return ConvertBuiltinToString<HOST_TYPE>::impl(State);

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
  
  return std::string();
}

/// \brief Get a string representation of State interpreted as the given array
///        type.
std::string
toString(clang::ConstantArrayType const *Type, llvm::ArrayRef<char> State) {
  auto ElemType = Type->getElementType();
  auto CanonElemType = ElemType->getCanonicalTypeUnqualified()->getTypePtr();
  
  auto &APSize = Type->getSize();
  assert(APSize.getBitWidth() <= 64);
  
  auto Size = APSize.getZExtValue();
  
  std::string RetStr;
  
  {
    llvm::raw_string_ostream Stream(RetStr);
    Stream << '{';
    
    if (Size) {
      // TODO: Get the size of the CanonElemType to index the State.
      auto ElemSize = State.size() / Size;
      assert(State.size() % Size == 0);
      
      Stream << toString(CanonElemType, State.slice(0, ElemSize));
      
      for (std::size_t i = 1; i < Size; ++i) {
        Stream << ", ";
        Stream << toString(CanonElemType, State.slice(i * ElemSize, ElemSize));
      }
    }
    
    Stream << '}';
  }
  
  return RetStr;
}

std::string
toString(clang::Type const *Type, llvm::ArrayRef<char> State) {
  switch (Type->getTypeClass()) {
    case clang::Type::Builtin:
      return toString(llvm::cast<clang::BuiltinType>(Type), State);
    
    case clang::Type::Pointer:
      return ConvertBuiltinToString<void const *>::impl(State);
    
    case clang::Type::ConstantArray:
      return toString(llvm::cast<clang::ConstantArrayType>(Type), State);
    
    default:
      break;
  }
  
  // TODO: describe type.
  return std::string("unhandled type");
}

// Documented in seec/Clang/StateMapping.hpp
std::string
toString(clang::ValueDecl const *Value, llvm::ArrayRef<char> State) {
  auto Type = Value->getType()->getCanonicalTypeUnqualified()->getTypePtr();
  return toString(Type, State);
}

/// @} (toString())


} // namespace seec_clang (in seec)

} // namespace seec
