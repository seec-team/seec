//===- StateMapping.cpp ---------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "seec/Clang/StateMapping.hpp"

#include "clang/AST/Decl.h"

#include "llvm/Support/raw_ostream.h"

#include <string>

namespace seec {

namespace seec_clang {


struct ConvertBuiltinToStringUnhandledType {};

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
struct ConvertBuiltinToString<void> {
  static std::string impl(llvm::ArrayRef<char> State) {
    return std::string("void");
  }
};

template<>
struct ConvertBuiltinToString<ConvertBuiltinToStringUnhandledType> {
  static std::string impl(llvm::ArrayRef<char> State) {
    return std::string("unhandled builtin");
  }
};


std::string
toString(clang::BuiltinType const *Type, llvm::ArrayRef<char> State) {
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN(KIND, HOST_TYPE)                \
    case clang::BuiltinType::KIND:                          \
      return ConvertBuiltinToString<HOST_TYPE>::impl(State);

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
SEEC_HANDLE_BUILTIN(UInt128, ConvertBuiltinToStringUnhandledType)

// Signed types
SEEC_HANDLE_BUILTIN(Char_S, char)
SEEC_HANDLE_BUILTIN(SChar, signed char)
SEEC_HANDLE_BUILTIN(WChar_S, wchar_t)
SEEC_HANDLE_BUILTIN(Short, short)
SEEC_HANDLE_BUILTIN(Int, int)
SEEC_HANDLE_BUILTIN(Long, long)
SEEC_HANDLE_BUILTIN(LongLong, long long)
SEEC_HANDLE_BUILTIN(Int128, ConvertBuiltinToStringUnhandledType)

// Floating point types
SEEC_HANDLE_BUILTIN(Half, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(Float, float)
SEEC_HANDLE_BUILTIN(Double, double)
SEEC_HANDLE_BUILTIN(LongDouble, ConvertBuiltinToStringUnhandledType)

// Language-specific types
SEEC_HANDLE_BUILTIN(NullPtr, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(ObjCId, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(ObjCClass, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(ObjCSel, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(Dependent, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(Overload, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(BoundMember, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(PseudoObject, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(UnknownAny, ConvertBuiltinToStringUnhandledType)
SEEC_HANDLE_BUILTIN(ARCUnbridgedCast, ConvertBuiltinToStringUnhandledType)

#undef SEEC_HANDLE_BUILTIN
  }
  
  return std::string();
}

std::string
toString(clang::ValueDecl const *Value, llvm::ArrayRef<char> State) {
  auto Type = Value->getType()->getCanonicalTypeUnqualified()->getTypePtr();
  
  switch (Type->getTypeClass()) {
    case clang::Type::Builtin:
      return toString(llvm::cast<clang::BuiltinType>(Type), State);
    
    case clang::Type::Pointer:
      return ConvertBuiltinToString<void const *>::impl(State);
    
    default:
      break;
  }
  
  return std::string("unhandled type");
}


} // namespace seec_clang (in seec)

} // namespace seec
