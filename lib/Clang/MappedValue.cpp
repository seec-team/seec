//===- lib/Clang/MappedValue.cpp ------------------------------------------===//
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

#define DEBUG_TYPE "seec-clang"

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedStmt.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Clang/TypeMatch.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ProcessState.hpp"
#include "seec/Trace/StreamState.hpp"
#include "seec/Trace/ThreadState.hpp"
#include "seec/Trace/FunctionState.hpp"
#include "seec/Trace/GetCurrentRuntimeValue.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/Range.hpp"

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/Frontend/ASTUnit.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <cctype>
#include <string>


namespace seec {

namespace cm {


/// \brief Used to store the initialization state of a Value.
///
enum class InitializationState {
  None,
  Partial,
  Complete
};


//===----------------------------------------------------------------------===//
// Value
//===----------------------------------------------------------------------===//

Value::~Value() = default;


//===----------------------------------------------------------------------===//
// getScalarValueAsString() - from memory
//===----------------------------------------------------------------------===//

template<typename T>
struct GetMemoryOfBuiltinAsString {
  static std::string impl(seec::trace::MemoryState::Region const &Region) {
    if (Region.getArea().length() != sizeof(T))
      return std::string("<size mismatch>");
    
    auto const Bytes = Region.getByteValues();
    return std::to_string(*reinterpret_cast<T const *>(Bytes.data()));
  }
};

template<>
struct GetMemoryOfBuiltinAsString<char> {
  static std::string impl(seec::trace::MemoryState::Region const &Region) {
    if (Region.getArea().length() != sizeof(char))
      return std::string("<size mismatch>");
    
    std::string RetStr;
    
    {
      llvm::raw_string_ostream Stream(RetStr);
      auto const Bytes = Region.getByteValues();
      auto const Character = *reinterpret_cast<char const *>(Bytes.data());
      
      if (std::isprint(Character))
        Stream << Character;
      else {
        Stream << '\\';
        
        switch (Character) {
          case '\t': Stream << 't'; break;
          case '\f': Stream << 'f'; break;
          case '\v': Stream << 'v'; break;
          case '\n': Stream << 'n'; break;
          case '\r': Stream << 'r'; break;
          default:   Stream << int(Character); break;
        }
      }
    }
    
    return RetStr;
  }
};

template<>
struct GetMemoryOfBuiltinAsString<void const *> {
  static std::string impl(seec::trace::MemoryState::Region const &Region) {
    if (Region.getArea().length() != sizeof(void const *))
      return std::string("<size mismatch>");
    
    std::string RetStr;
    
    {
      llvm::raw_string_ostream Stream(RetStr);
      auto const Bytes = Region.getByteValues();
      Stream << *reinterpret_cast<void const * const *>(Bytes.data());
    }
    
    return RetStr;
  }
};

template<>
struct GetMemoryOfBuiltinAsString<void> {
  static std::string impl(seec::trace::MemoryState::Region const &Region) {
    return std::string("<void>");
  }
};

std::string
getScalarValueAsString(::clang::BuiltinType const *Type,
                       seec::trace::MemoryState::Region const &Region)
{
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN(KIND, HOST_TYPE)                                   \
    case clang::BuiltinType::KIND:                                             \
      return GetMemoryOfBuiltinAsString<HOST_TYPE>::impl(Region);

#define SEEC_UNHANDLED_BUILTIN(KIND)                                           \
    case clang::BuiltinType::KIND:                                             \
      return std::string("<builtin \"" #KIND "\" not implemented>"); 

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
    SEEC_UNHANDLED_BUILTIN(OCLSampler)
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
  
  llvm_unreachable("unexpected builtin.");
  return std::string("<unexpected builtin>");
}

std::string
getScalarValueAsString(::clang::Type const *Type,
                       seec::trace::MemoryState::Region const &Region)
{
  auto const CanonQualType = Type->getCanonicalTypeInternal();
  auto const CanonType = CanonQualType.getTypePtr();
  
  switch (CanonQualType->getTypeClass()) {
    // BuiltinType
    case ::clang::Type::Builtin:
    {
      auto const Ty = llvm::cast< ::clang::BuiltinType>(CanonType);
      return getScalarValueAsString(Ty, Region);
    }
    
    // AtomicType
    case ::clang::Type::Atomic:
    {
      // Recursive on the underlying type.
      auto const Ty = llvm::cast< ::clang::AtomicType>(CanonType);
      auto const ValueTy = Ty->getValueType().getCanonicalType().getTypePtr();
      return getScalarValueAsString(ValueTy, Region);
    }
    
    // EnumType
    case ::clang::Type::Enum:
    {
      // Recursive on the underlying type.
      auto const Ty = llvm::cast< ::clang::EnumType>(CanonType);
      auto const IntegerTy = Ty->getDecl()->getIntegerType();
      auto const CanonicalIntegerTy = IntegerTy.getCanonicalType().getTypePtr();
      return getScalarValueAsString(CanonicalIntegerTy, Region);
    }
    
    // PointerType
    case ::clang::Type::Pointer:
    {
      return GetMemoryOfBuiltinAsString<void const *>::impl(Region);
    }
    
#define SEEC_UNHANDLED_TYPE_CLASS(CLASS)                                       \
    case ::clang::Type::CLASS:                                                 \
      return std::string("<type class " #CLASS " not implemented>");
    
    SEEC_UNHANDLED_TYPE_CLASS(Complex) // TODO.
    SEEC_UNHANDLED_TYPE_CLASS(Record) // TODO.
    SEEC_UNHANDLED_TYPE_CLASS(ConstantArray) // TODO.
    SEEC_UNHANDLED_TYPE_CLASS(IncompleteArray) // TODO.
    SEEC_UNHANDLED_TYPE_CLASS(VariableArray) // TODO.
    
    // Not needed because we don't support the language(s).
    SEEC_UNHANDLED_TYPE_CLASS(BlockPointer) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(LValueReference) // C++
    SEEC_UNHANDLED_TYPE_CLASS(RValueReference) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(MemberPointer) // C++
    SEEC_UNHANDLED_TYPE_CLASS(Auto) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObject) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCInterface) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObjectPointer) //ObjC
    
    SEEC_UNHANDLED_TYPE_CLASS(Vector) // GCC extension
    SEEC_UNHANDLED_TYPE_CLASS(ExtVector) // Extension
    SEEC_UNHANDLED_TYPE_CLASS(FunctionProto)
    SEEC_UNHANDLED_TYPE_CLASS(FunctionNoProto)

    // This function only operates on canonical types, so we
    // automatically ignore non-canonical types, dependent types, and
    // non-canonical-unless-dependent types.
#define TYPE(CLASS, BASE)

#define ABSTRACT_TYPE(CLASS, BASE)

#define NON_CANONICAL_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#define DEPENDENT_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#include "clang/AST/TypeNodes.def"

#undef SEEC_UNHANDLED_TYPE_CLASS
  }
  
  llvm_unreachable("unhandled type class");
  return std::string("<unhandled type class>");
}


//===----------------------------------------------------------------------===//
// ValueByMemoryForScalar
//===----------------------------------------------------------------------===//

/// \brief Represents a simple scalar Value in memory.
///
class ValueByMemoryForScalar final : public ValueOfScalar {
  /// The canonical Type of this value.
  ::clang::Type const * CanonicalType;
  
  /// The recorded memory address of the value.
  uintptr_t Address;
  
  /// The size of the value.
  ::clang::CharUnits Size;
  
  /// The state of recorded memory.
  seec::trace::MemoryState const &Memory;
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return Size;
  }
  
  /// \brief Check if this value is zero.
  ///
  /// pre: isCompletelyInitialized() == true
  ///
  virtual bool isZeroImpl() const override {
    auto const Region = Memory.getRegion(MemoryArea(Address,
                                                    Size.getQuantity()));
    
    for (auto const Byte: Region.getByteValues())
      if (Byte)
        return false;
    
    return true;
  }
  
public:
  /// \brief Constructor.
  ///
  ValueByMemoryForScalar(::clang::Type const *WithCanonicalType,
                         uintptr_t WithAddress,
                         ::clang::CharUnits WithSize,
                         seec::trace::ProcessState const &ForProcessState)
  : ValueOfScalar(),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    Size(WithSize),
    Memory(ForProcessState.getMemory())
  {}
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return CanonicalType;
  }
  
  /// \brief In-memory scalar values never have an associated Expr.
  /// \return nullptr.
  ///
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  /// \brief In-memory values are always in memory.
  /// \return true.
  ///
  virtual bool isInMemory() const override { return true; }
  
  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual uintptr_t getAddress() const override { return Address; }
  
  virtual bool isCompletelyInitialized() const override {
    auto Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
    return Region.isCompletelyInitialized();
  }
  
  virtual bool isPartiallyInitialized() const override {
    auto Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
    
    // TODO: We should implement this in MemoryRegion, because it will be much
    //       more efficient.
    for (auto Value : Region.getByteInitialization())
      if (Value)
        return true;
    
    return false;
  }
  
  virtual std::string getValueAsStringShort() const override {
    if (!isCompletelyInitialized())
      return std::string("<uninitialized>");
    
    auto const Length = Size.getQuantity();
    
    return getScalarValueAsString(CanonicalType,
                                  Memory.getRegion(MemoryArea(Address,
                                                              Length)));
  }
  
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForPointer
//===----------------------------------------------------------------------===//

/// \brief Represents a pointer Value in LLVM's virtual registers.
///
class ValueByMemoryForPointer final : public ValueOfPointer {
  /// The Store for this Value.
  std::weak_ptr<ValueStore const> Store;
  
  /// The Context for this Value.
  ::clang::ASTContext const &ASTContext;
  
  /// The canonical Type of this value.
  ::clang::Type const *CanonicalType;
  
  /// The address of this pointer (not the value of the pointer).
  uintptr_t Address;
  
  /// The size of the pointee type.
  ::clang::CharUnits PointeeSize;
  
  /// The raw value of this pointer.
  uintptr_t RawValue;
  
  /// The ProcessState that this value is for.
  seec::trace::ProcessState const &ProcessState;
  
  /// \brief Constructor.
  ///
  ValueByMemoryForPointer(std::weak_ptr<ValueStore const> InStore,
                          ::clang::ASTContext const &WithASTContext,
                          ::clang::Type const *WithCanonicalType,
                          uintptr_t WithAddress,
                          ::clang::CharUnits WithPointeeSize,
                          uintptr_t WithRawValue,
                          seec::trace::ProcessState const &ForProcessState)
  : Store(InStore),
    ASTContext(WithASTContext),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    PointeeSize(WithPointeeSize),
    RawValue(WithRawValue),
    ProcessState(ForProcessState)
  {}
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ASTContext.getTypeSizeInChars(CanonicalType);
  }
  
  /// \brief Check if this is a valid opaque pointer (e.g. a DIR *).
  ///
  virtual bool isValidOpaqueImpl() const override {
    return ProcessState.getDir(RawValue) != nullptr;
  }
  
  /// \brief Get the raw value of this pointer.
  ///
  virtual uintptr_t getRawValueImpl() const override {
    return RawValue;
  }
  
  /// \brief Get the size of the pointee type.
  ///
  virtual ::clang::CharUnits getPointeeSizeImpl() const override {
    return PointeeSize;
  }
  
public:
  /// \brief Attempt to create a new ValueByMemoryForPointer.
  ///
  static std::shared_ptr<ValueByMemoryForPointer const>
  create(std::weak_ptr<ValueStore const> Store,
         ::clang::ASTContext const &ASTContext,
         ::clang::Type const *CanonicalType,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
  {
    // Get the size of pointee type.
    auto const Type = CanonicalType->getAs< ::clang::PointerType>();
    assert(Type && "Expected PointerType");
    
    auto const PointeeQType = Type->getPointeeType().getCanonicalType();
    auto const PointeeSize = PointeeQType->isIncompleteType()
                           ? ::clang::CharUnits::fromQuantity(0)
                           : ASTContext.getTypeSizeInChars(PointeeQType);
    
    // Calculate the raw pointer value (don't worry if the memory is
    // uninitialized: getByteValues() will return zeros and we simply won't use
    // the calculated value).
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(void const *)));
    auto const RawBytes = Region.getByteValues();
    auto const PtrValue = *reinterpret_cast<uintptr_t const *>(RawBytes.data());
    
    // Create the object.
    return std::shared_ptr<ValueByMemoryForPointer const>
                          (new ValueByMemoryForPointer(Store,
                                                       ASTContext,
                                                       CanonicalType,
                                                       Address,
                                                       PointeeSize,
                                                       PtrValue,
                                                       ProcessState));
  }
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return CanonicalType;
  }
  
  /// \brief Get the Expr that this Value is for.
  ///
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  virtual bool isInMemory() const override { return true; }
  
  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual uintptr_t getAddress() const override { return Address; }
  
  virtual bool isCompletelyInitialized() const override {
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(void const *)));
    return Region.isCompletelyInitialized();
  }
  
  virtual bool isPartiallyInitialized() const override {
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(void const *)));
    
    // TODO: We should implement this in MemoryRegion, because it will be much
    //       more efficient.
    for (auto Value : Region.getByteInitialization())
      if (Value)
        return true;
    
    return false;
  }
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  virtual std::string getValueAsStringShort() const override {
    if (!isCompletelyInitialized())
      return std::string("<uninitialized>");
    
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(void const *)));
    return getScalarValueAsString(CanonicalType, Region);
  }
  
  /// Get a string describing the value.
  ///
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
  
  /// \brief Get the highest legal dereference of this value.
  ///
  virtual unsigned getDereferenceIndexLimit() const override {
    if (!isCompletelyInitialized())
      return 0;
    
    // TODO: Move these calculations into the construction process.
    auto const MaybeArea = ProcessState.getContainingMemoryArea(RawValue);
    if (!MaybeArea.assigned<MemoryArea>())
      return 0;
    
    if (PointeeSize.isZero())
      return 0;
    
    // If the pointee is a struct with a flexible array member, then we should
    // never allow more than one dereference, because the additional memory (if
    // any) is occupied by the flexible array member.
    auto const PointeeTy = CanonicalType->getPointeeType();
    auto const RecordTy = PointeeTy->getAs< ::clang::RecordType >();
    
    if (RecordTy) {
      auto const RecordDecl = RecordTy->getDecl()->getDefinition();
      if (RecordDecl) {
        if (RecordDecl->hasFlexibleArrayMember()) {
          return 1;
        }
      }
    }
    
    auto const Area = MaybeArea.get<MemoryArea>().withStart(RawValue);
    return Area.length() / PointeeSize.getQuantity();
  }
  
  /// \brief Get the value of this pointer dereferenced using the given Index.
  ///
  virtual std::shared_ptr<Value const>
  getDereferenced(unsigned Index) const override {
    // Find the Store (if it still exists).
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<Value const>();
    
    auto const Address = RawValue + (Index * PointeeSize.getQuantity());
    
    return getValue(StorePtr,
                    CanonicalType->getPointeeType(),
                    ASTContext,
                    Address,
                    ProcessState);
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForPointerToFILE
//===----------------------------------------------------------------------===//

class ValueByMemoryForPointerToFILE final : public ValueOfPointerToFILE {
  /// The canonical type of this Value.
  ::clang::PointerType const *CanonType;
  
  /// The address of this Value in memory.
  uintptr_t Address;
  
  /// The raw value of this FILE pointer.
  uintptr_t RawValue;
  
  /// The initialization state of this Value.
  InitializationState Initialized;
  
  /// The (unmapped) StreamState for this FILE, or nullptr if it is invalid.
  seec::trace::StreamState const *Stream;
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ::clang::CharUnits::fromQuantity(sizeof(void *));
  }
  
  /// \brief Get the raw value of this pointer.
  ///
  uintptr_t getRawValueImpl() const override {
    assert(Initialized == InitializationState::Complete);
    return RawValue;
  }
  
  /// \brief Check whether this FILE pointer is valid (an open stream).
  ///
  bool isValidImpl() const override { return Stream != nullptr; }
  
  /// \brief Constructor.
  ///
  ValueByMemoryForPointerToFILE(::clang::PointerType const *WithCanonType,
                                uintptr_t const WithAddress,
                                uintptr_t const WithRawValue,
                                InitializationState const WithInitialized,
                                seec::trace::StreamState const *WithStream)
  : CanonType(WithCanonType),
    Address(WithAddress),
    RawValue(WithRawValue),
    Initialized(WithInitialized),
    Stream(WithStream)
  {}
  
public:
  /// \brief Attempt to create a new ValueByMemoryForPointer.
  ///
  static std::shared_ptr<ValueByMemoryForPointerToFILE const>
  create(::clang::Type const * const CanonicalType,
         uintptr_t const Address,
         seec::trace::ProcessState const &ProcessState)
  {
    // Get the size of pointer type.
    auto const Type = CanonicalType->getAs< ::clang::PointerType >();
    assert(Type && "Expected PointerType");
    
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(uintptr_t)));
    
    // Determine the initialization state of the memory.
    InitializationState const Initialized = Region.isCompletelyInitialized()
                                          ? InitializationState::Complete
                                          : (Region.isPartiallyInitialized()
                                             ? InitializationState::Partial 
                                             : InitializationState::None);
    
    // Calculate the raw pointer value (don't worry if the memory is
    // uninitialized: getByteValues() will return zeros and we simply won't use
    // the calculated value).
    auto const RawBytes = Region.getByteValues();
    auto const PtrValue = *reinterpret_cast<uintptr_t const *>(RawBytes.data());
    
    // Check whether the stream is valid.
    auto const Stream = ProcessState.getStream(PtrValue);
    
    // Create the object.
    return std::shared_ptr<ValueByMemoryForPointerToFILE const>
                          (new ValueByMemoryForPointerToFILE{
                                Type,
                                Address,
                                PtrValue,
                                Initialized,
                                Stream
                               });
  }
  
  /// \brief Get the canonical type of this Value.
  ///
  ::clang::Type const *getCanonicalType() const override {
    return CanonType;
  }
  
  /// \brief Get the Expr that this Value is for (if any).
  ///
  ::clang::Expr const *getExpr() const override { return nullptr; }
  
  /// \brief Check if this represents a value stored in memory.
  ///
  bool isInMemory() const override { return true; }
  
  /// \brief Get the address in memory.
  /// 
  /// pre: isInMemory() == true
  ///
  uintptr_t getAddress() const override { return Address; }
  
  /// \brief Check if this value is completely initialized.
  ///
  bool isCompletelyInitialized() const override {
    return Initialized == InitializationState::Complete;
  }
  
  /// \brief Check if this value is partially initialized.
  ///
  bool isPartiallyInitialized() const override {
    return Initialized == InitializationState::Partial;
  }
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  std::string getValueAsStringShort() const override {
    return getValueAsStringFull();
  }
  
  /// \brief Get a string describing the value.
  ///
  std::string getValueAsStringFull() const override {
    if (Initialized != InitializationState::Complete)
      return std::string("<uninitialized>");
    
    if (!Stream)
      return std::string("<invalid FILE>");
    
    std::string Value = "FILE ";
    Value += Stream->getFilename();
    return Value;
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForRecord
//===----------------------------------------------------------------------===//

/// \brief Represents a record Value in memory.
///
class ValueByMemoryForRecord final : public ValueOfRecord {
  /// The Store for this Value.
  std::weak_ptr<ValueStore const> Store;
  
  /// The Context for this Value.
  ::clang::ASTContext const &ASTContext;
  
  /// The layout information for this Record.
  ::clang::ASTRecordLayout const &Layout;
  
  /// The canonical Type of this value.
  ::clang::Type const *CanonicalType;
  
  /// The memory address of this Value.
  uintptr_t Address;
  
  /// The process state that this Value is in.
  seec::trace::ProcessState const &ProcessState;
  
  /// \brief Constructor.
  ///
  ValueByMemoryForRecord(std::weak_ptr<ValueStore const> InStore,
                         ::clang::ASTContext const &WithASTContext,
                         ::clang::ASTRecordLayout const &WithLayout,
                         ::clang::Type const *WithCanonicalType,
                         uintptr_t WithAddress,
                         seec::trace::ProcessState const &ForProcessState)
  : Store(InStore),
    ASTContext(WithASTContext),
    Layout(WithLayout),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    ProcessState(ForProcessState)
  {}
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ASTContext.getTypeSizeInChars(CanonicalType);
  }
  
public:
  /// \brief Attempt to create a new instance of this class.
  ///
  static std::shared_ptr<ValueByMemoryForRecord>
  create(std::weak_ptr<ValueStore const> Store,
         ::clang::ASTContext const &ASTContext,
         ::clang::Type const *CanonicalType,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
  {
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonicalType);
    auto const Decl = RecordTy->getDecl()->getDefinition();
    if (!Decl)
      return std::shared_ptr<ValueByMemoryForRecord>();
    
    auto const &Layout = ASTContext.getASTRecordLayout(Decl);
    
    return std::shared_ptr<ValueByMemoryForRecord>
                          (new ValueByMemoryForRecord(Store,
                                                      ASTContext,
                                                      Layout,
                                                      CanonicalType,
                                                      Address,
                                                      ProcessState));
  }
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return CanonicalType;
  }
  
  /// \brief In-memory values are never for an Expr.
  /// \return nullptr.
  ///
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  /// \brief In-memory values are always in memory.
  /// \return true.
  ///
  virtual bool isInMemory() const override { return true; }
  
  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual uintptr_t getAddress() const override { return Address; }
  
  /// \brief Check if this value is completely initialized.
  ///
  /// If this is an aggregate value, then the result of this method is the
  /// logical AND reduction of applying this operation to all children.
  ///
  virtual bool isCompletelyInitialized() const override {
    for (unsigned i = 0, Count = getChildCount(); i < Count; ++i) {
      auto const Child = getChildAt(i);
      if (Child && !Child->isCompletelyInitialized())
        return false;
    }
    
    return true;
  }
  
  /// \brief Check if this value is partially initialized.
  ///
  /// If this is an aggregate value, then the result of this method is the
  /// logical OR reduction of applying this operation to all children.
  ///
  virtual bool isPartiallyInitialized() const override {
    for (unsigned i = 0, Count = getChildCount(); i < Count; ++i) {
      auto const Child = getChildAt(i);
      if (Child && Child->isPartiallyInitialized())
        return true;
    }
    
    return false;
  }
  
  /// \brief Get a string representing an elided struct: "{ ... }"
  ///
  virtual std::string getValueAsStringShort() const override {
    return std::string("{ ... }");
  }
  
  /// \brief Get a string describing the full value of all child members.
  ///
  virtual std::string getValueAsStringFull() const override {
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonicalType);
    auto const Decl = RecordTy->getDecl()->getDefinition();
    
    std::string ValueStr;
    
    {
      llvm::raw_string_ostream Stream(ValueStr);
      bool FirstField = true;
      
      Stream << '{';
      
      for (auto const Field : seec::range(Decl->field_begin(),
                                          Decl->field_end()))
      {
        auto const Child = getChildAt(Field->getFieldIndex());
        if (!Child)
          continue;
        
        if (!FirstField) {
          Stream << ',';
        }
        else {
          FirstField = false;
        }
        
        Stream << " ."
               << Field->getNameAsString()
               << " = "
               << Child->getValueAsStringFull();
      }
      
      Stream << " }";
    }
    
    return ValueStr;
  }
  
  /// \brief Get the number of members of this record.
  ///
  virtual unsigned getChildCount() const override {
    return Layout.getFieldCount();
  }
  
  /// \brief Get the FieldDecl for the given child.
  ///
  virtual ::clang::FieldDecl const *
  getChildField(unsigned Index) const override {
    if (Index > Layout.getFieldCount())
      return nullptr;
    
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonicalType);
    auto const Decl = RecordTy->getDecl()->getDefinition();
    
    auto FieldIt = Decl->field_begin();
    std::advance(FieldIt, Index);
    
    return *FieldIt;
  }
  
  /// \brief Get the Value of a member of this record.
  ///
  virtual std::shared_ptr<Value const>
  getChildAt(unsigned Index) const override {
    assert(Index < getChildCount() && "Invalid Child Index");
    
    // Get the store (if it still exists).
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<Value const>();
    
    // Get information about the Index-th field.
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonicalType);
    auto const Decl = RecordTy->getDecl()->getDefinition();
    
    auto FieldIt = Decl->field_begin();
    for (auto FieldEnd = Decl->field_end(); ; ++FieldIt) {
      if (FieldIt == FieldEnd)
        return std::shared_ptr<Value const>();
      
      if (FieldIt->getFieldIndex() == Index)
        break;
    }
    
    // TODO: We don't support bitfields yet!
    auto const BitOffset = Layout.getFieldOffset(Index);
    if (BitOffset % CHAR_BIT != 0)
      return std::shared_ptr<Value const>();
    
    return getValue(StorePtr,
                    FieldIt->getType(),
                    ASTContext,
                    Address + (BitOffset / CHAR_BIT),
                    ProcessState);
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForArray
//===----------------------------------------------------------------------===//

/// \brief Represents an array Value in memory.
///
class ValueByMemoryForArray final : public ValueOfArray {
  /// The Store for this Value.
  std::weak_ptr<ValueStore const> Store;
  
  /// The Context for this Value.
  ::clang::ASTContext const &ASTContext;
  
  /// The canonical Type of this value.
  ::clang::ArrayType const *CanonicalType;
  
  /// The memory address of this Value.
  uintptr_t Address;
  
  /// The size of an element.
  unsigned ElementSize;
  
  /// The number of elements.
  unsigned ElementCount;
  
  /// The process state that this Value is in.
  seec::trace::ProcessState const &ProcessState;

  /// \brief Constructor.
  ///
  ValueByMemoryForArray(std::weak_ptr<ValueStore const> InStore,
                        ::clang::ASTContext const &WithASTContext,
                        ::clang::ArrayType const *WithCanonicalType,
                        uintptr_t WithAddress,
                        unsigned WithElementSize,
                        unsigned WithElementCount,
                        seec::trace::ProcessState const &ForProcessState)
  : Store(InStore),
    ASTContext(WithASTContext),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    ElementSize(WithElementSize),
    ElementCount(WithElementCount),
    ProcessState(ForProcessState)
  {}
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ASTContext.getTypeSizeInChars(CanonicalType);
  }
  
public:
  /// \brief Attempt to create a new instance of this class.
  ///
  static std::shared_ptr<ValueByMemoryForArray const>
  create(std::weak_ptr<ValueStore const> Store,
         ::clang::ASTContext const &ASTContext,
         ::clang::Type const *CanonicalType,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
  {
    auto const ArrayTy = llvm::cast< ::clang::ArrayType>(CanonicalType);
    auto const ElementTy = ArrayTy->getElementType();
    auto const ElementSize = ASTContext.getTypeSizeInChars(ElementTy);
    
    if (ElementSize.isZero()) {
      return std::shared_ptr<ValueByMemoryForArray const>();
    }
    
    unsigned ElementCount = 0;
    
    switch (ArrayTy->getTypeClass()) {
      case ::clang::Type::TypeClass::ConstantArray:
      {
        auto const Ty = llvm::cast< ::clang::ConstantArrayType>(ArrayTy);
        ElementCount = Ty->getSize().getZExtValue();
        
        break;
      }
      
      // We could attempt to get the runtime value generated by the size
      // expression, but we would need access to the function state. Instead,
      // just use whatever size fills the allocated memory block, as we do
      // for IncompleteArray types.
      case ::clang::Type::TypeClass::VariableArray: SEEC_FALLTHROUGH;
      case ::clang::Type::TypeClass::IncompleteArray:
      {
        auto const MaybeArea = ProcessState.getContainingMemoryArea(Address);
        if (MaybeArea.assigned<MemoryArea>()) {
          auto const Area = MaybeArea.get<MemoryArea>().withStart(Address);
          ElementCount = Area.length() / ElementSize.getQuantity();
        }
        
        break;
      }
      
      // Not implemented!
      case ::clang::Type::TypeClass::DependentSizedArray: SEEC_FALLTHROUGH;
      default:
        llvm_unreachable("not implemented");
        return std::shared_ptr<ValueByMemoryForArray const>();
    }
    
    return std::shared_ptr<ValueByMemoryForArray const>
                          (new ValueByMemoryForArray(Store,
                                                     ASTContext,
                                                     ArrayTy,
                                                     Address,
                                                     ElementSize.getQuantity(),
                                                     ElementCount,
                                                     ProcessState));
  }
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return CanonicalType;
  }
  
  /// \brief In-memory values are never for an Expr.
  /// \return nullptr.
  ///
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  /// \brief In-memory values are always in memory.
  /// \return true.
  ///
  virtual bool isInMemory() const override { return true; }
  
  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual uintptr_t getAddress() const override { return Address; }
  
  /// \brief Check if this value is completely initialized.
  ///
  /// If this is an aggregate value, then the result of this method is the
  /// logical AND reduction of applying this operation to all children.
  ///
  virtual bool isCompletelyInitialized() const override {
    for (unsigned i = 0, Count = getChildCount(); i < Count; ++i) {
      auto const Child = getChildAt(i);
      if (Child && !Child->isCompletelyInitialized())
        return false;
    }
    
    return true;
  }
  
  /// \brief Check if this value is partially initialized.
  ///
  /// If this is an aggregate value, then the result of this method is the
  /// logical OR reduction of applying this operation to all children.
  ///
  virtual bool isPartiallyInitialized() const override {
    for (unsigned i = 0, Count = getChildCount(); i < Count; ++i) {
      auto const Child = getChildAt(i);
      if (Child && Child->isPartiallyInitialized())
        return true;
    }
    
    return false;
  }
  
  virtual std::string getValueAsStringShort() const override {
    return std::string("[ ... ]");
  }
  
  virtual std::string getValueAsStringFull() const override {
    if (ElementCount == 0)
      return std::string("[]");
    
    std::string ValueStr;
    
    {
      llvm::raw_string_ostream Stream(ValueStr);
      
      Stream << "[";
      
      for (unsigned i = 0; i < ElementCount; ++i) {
        if (i != 0)
          Stream << ", ";
        
        auto const Child = getChildAt(i);
        if (!Child) {
          Stream << "<error>";
          continue;
        }
        
        Stream << Child->getValueAsStringFull();
      }
      
      Stream << "]";
    }
    
    return ValueStr;
  }
  
  virtual unsigned getChildCount() const override {
    return ElementCount;
  }
  
  virtual std::shared_ptr<Value const>
  getChildAt(unsigned Index) const override {
    assert(Index < ElementCount && "Invalid Child Index");
    
    // Get the store (if it still exists).
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<Value const>();
    
    auto const ChildAddress = Address + (Index * ElementSize);
    
    return getValue(StorePtr,
                    CanonicalType->getElementType(),
                    ASTContext,
                    ChildAddress,
                    ProcessState);
  }
};


//===----------------------------------------------------------------------===//
// getScalarValueAsAPSInt() - from llvm::Value
//===----------------------------------------------------------------------===//

template<typename T>
struct GetValueOfBuiltinAsAPSInt {
  static seec::Maybe<llvm::APSInt>
  impl(seec::trace::FunctionState const &State, ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getCurrentRuntimeValueAs<T>
                                                                 (State, Value);
    if (!MaybeValue.assigned())
      return seec::Maybe<llvm::APSInt>();
    
    llvm::APSInt APSValue(sizeof(T) * CHAR_BIT, std::is_unsigned<T>::value);
    APSValue = MaybeValue.template get<0>();
    
    return APSValue;
  }
};

seec::Maybe<llvm::APSInt>
getScalarValueAsAPSInt(seec::trace::FunctionState const &State,
                       ::clang::BuiltinType const *Type,
                       ::llvm::Value const *Value)
{
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN(KIND, HOST_TYPE)                                   \
    case clang::BuiltinType::KIND:                                             \
      return GetValueOfBuiltinAsAPSInt<HOST_TYPE>::impl(State, Value);

#define SEEC_UNHANDLED_BUILTIN(KIND)                                           \
    case clang::BuiltinType::KIND:                                             \
      return seec::Maybe<llvm::APSInt>();

    // Builtin types
    SEEC_UNHANDLED_BUILTIN(Void)
    
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
    SEEC_UNHANDLED_BUILTIN(Float)
    SEEC_UNHANDLED_BUILTIN(Double)
    SEEC_UNHANDLED_BUILTIN(LongDouble)
    
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
    SEEC_UNHANDLED_BUILTIN(OCLSampler)
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
  
  llvm_unreachable("unexpected builtin type");
  return seec::Maybe<llvm::APSInt>();
}

seec::Maybe<llvm::APSInt>
getScalarValueAsAPSInt(seec::trace::FunctionState const &State,
                       ::clang::Type const *Type,
                       ::llvm::Value const *Value)
{
  switch (Type->getTypeClass()) {
    // BuiltinType
    case ::clang::Type::Builtin:
      return getScalarValueAsAPSInt(State,
                                    llvm::cast< ::clang::BuiltinType>(Type),
                                    Value);
    
    // AtomicType
    case ::clang::Type::Atomic:
    {
      auto const AtomicTy = llvm::cast< ::clang::AtomicType>(Type);
      return getScalarValueAsAPSInt(State,
                                    AtomicTy->getValueType().getTypePtr(),
                                    Value);
    }
    
    // EnumType
    case ::clang::Type::Enum:
    {
      auto const EnumTy = llvm::cast< ::clang::EnumType>(Type);
      auto const EnumDecl = EnumTy->getDecl();
      auto const UnderlyingTy = EnumDecl->getIntegerType().getTypePtr();
      return getScalarValueAsAPSInt(State, UnderlyingTy, Value);
    }
    
    // No other types are supported.
    default:
      return seec::Maybe<llvm::APSInt>();
  }
}


//===----------------------------------------------------------------------===//
// getScalarValueAsString() - from llvm::Value
//===----------------------------------------------------------------------===//

template<typename T>
struct GetValueOfBuiltinAsString {
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getCurrentRuntimeValueAs<T>
                                                                 (State, Value);
    if (!MaybeValue.assigned()) {
      return std::string("<")
             + __PRETTY_FUNCTION__
             + ": couldn't get current runtime value>";
    }
    
    return std::to_string(MaybeValue.template get<0>());
  }
};

template<>
struct GetValueOfBuiltinAsString<void const *> {
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getCurrentRuntimeValueAs<void const *>
                                                                 (State, Value);
    if (!MaybeValue.assigned())
      return std::string("<void const *: couldn't get current runtime value>");
    
    std::string RetStr;
    
    {
      llvm::raw_string_ostream Stream(RetStr);
      Stream << MaybeValue.get<0>();
    }
    
    return RetStr;
  }
};

template<>
struct GetValueOfBuiltinAsString<void> {
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    return std::string("<void>");
  }
};

std::string getScalarValueAsString(seec::trace::FunctionState const &State,
                                   ::clang::BuiltinType const *Type,
                                   ::llvm::Value const *Value)
{
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN(KIND, HOST_TYPE)                                   \
    case clang::BuiltinType::KIND:                                             \
      return GetValueOfBuiltinAsString<HOST_TYPE>::impl(State, Value);

#define SEEC_UNHANDLED_BUILTIN(KIND)                                           \
    case clang::BuiltinType::KIND:                                             \
      return std::string("<unhandled builtin \"" #KIND "\">");

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
    SEEC_UNHANDLED_BUILTIN(OCLSampler)
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
  
  llvm_unreachable("unexpected builtin type");
  return std::string();
}

std::string getScalarValueAsString(seec::trace::FunctionState const &State,
                                   ::clang::Type const *Type,
                                   ::llvm::Value const *Value)
{
  switch (Type->getTypeClass()) {
    // BuiltinType
    case ::clang::Type::Builtin:
      return getScalarValueAsString(State,
                                    llvm::cast< ::clang::BuiltinType>(Type),
                                    Value);
    
    // AtomicType
    case ::clang::Type::Atomic:
    {
      auto const AtomicTy = llvm::cast< ::clang::AtomicType>(Type);
      return getScalarValueAsString(State,
                                    AtomicTy->getValueType().getTypePtr(),
                                    Value);
    }
    
    // EnumType
    case ::clang::Type::Enum:
    {
      auto const EnumTy = llvm::cast< ::clang::EnumType>(Type);
      
      auto const EnumDecl = EnumTy->getDecl();
      auto const UnderlyingTy = EnumDecl->getIntegerType().getTypePtr();
      assert(UnderlyingTy && "EnumDecl->getIntegerType() failed");
      
      // If we have the definition, perhaps we can print a "nicer" value.
      if (auto const EnumDef = EnumDecl->getDefinition()) {
        // Attempt to match the current value to the enum values.
        auto const MaybeIntVal = getScalarValueAsAPSInt(State,
                                                        UnderlyingTy,
                                                        Value);
        
        if (MaybeIntVal.assigned<llvm::APSInt>()) {
          auto const &IntVal = MaybeIntVal.get<llvm::APSInt>();
          std::string StringVal;
          
          for (auto const Decl : seec::range(EnumDef->enumerator_begin(),
                                             EnumDef->enumerator_end()))
          {
            if (llvm::APSInt::isSameValue(Decl->getInitVal(), IntVal)) {
              if (!StringVal.empty()) {
                StringVal += ", ";
              }
              else {
                StringVal += "(";
                StringVal += EnumDecl->getNameAsString();
                StringVal += ")";
              }
              
              StringVal += Decl->getNameAsString();
            }
          }
          
          // TODO: We could attempt to break down unrecognised values into
          //       bitwise and combinations (for flags).
          
          if (!StringVal.empty())
            return StringVal;
        }
      }
      
      // Just print the underlying integer value.
      return getScalarValueAsString(State, UnderlyingTy, Value);
    }
    
    // PointerType
    case ::clang::Type::Pointer:
    {
      auto const MaybeInt = GetValueOfBuiltinAsAPSInt<uintptr_t>::impl(State,
                                                                       Value);
      if (!MaybeInt.assigned<llvm::APSInt>())
        return std::string("<pointer: couldn't get value>");
      
      return std::string("0x") + MaybeInt.get<llvm::APSInt>().toString(16);
    }
    
#define SEEC_UNHANDLED_TYPE_CLASS(CLASS)                                       \
    case ::clang::Type::CLASS:                                                 \
      return std::string("<unhandled type class: " #CLASS ">");
    
    SEEC_UNHANDLED_TYPE_CLASS(Complex) // TODO?
    
    SEEC_UNHANDLED_TYPE_CLASS(Record) // structs/unions/classes
    
    // Not needed because we don't support the language(s).
    SEEC_UNHANDLED_TYPE_CLASS(BlockPointer) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(LValueReference) // C++
    SEEC_UNHANDLED_TYPE_CLASS(RValueReference) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(MemberPointer) // C++
    SEEC_UNHANDLED_TYPE_CLASS(Auto) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObject) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCInterface) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObjectPointer) //ObjC
    
    // May not be needed because we are only interested in runtime values (not
    // in-memory values)?
    SEEC_UNHANDLED_TYPE_CLASS(ConstantArray)
    SEEC_UNHANDLED_TYPE_CLASS(IncompleteArray)
    SEEC_UNHANDLED_TYPE_CLASS(VariableArray)
    SEEC_UNHANDLED_TYPE_CLASS(Vector) // GCC extension
    SEEC_UNHANDLED_TYPE_CLASS(ExtVector) // Extension
    SEEC_UNHANDLED_TYPE_CLASS(FunctionProto)
    SEEC_UNHANDLED_TYPE_CLASS(FunctionNoProto)

    // This function only operates on canonical scalar types, so we
    // automatically ignore non-canonical types, dependent types, and
    // non-canonical-unless-dependent types.
#define TYPE(CLASS, BASE)

#define ABSTRACT_TYPE(CLASS, BASE)

#define NON_CANONICAL_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#define DEPENDENT_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#include "clang/AST/TypeNodes.def"

#undef SEEC_UNHANDLED_TYPE_CLASS
  }
  
  llvm_unreachable("unexpected type class");
  return std::string();
}


//===----------------------------------------------------------------------===//
// ValueByRuntimeValueForScalar
//===----------------------------------------------------------------------===//

/// \brief Represents a simple scalar Value in LLVM's virtual registers.
///
class ValueByRuntimeValueForScalar final : public ValueOfScalar {
  /// The Expr that this value is for.
  ::clang::Expr const *Expression;
  
  /// The FunctionState that this value is for.
  seec::trace::FunctionState const &FunctionState;
  
  /// The LLVM Value for this value.
  llvm::Value const *LLVMValue;
  
  /// The size of this value.
  ::clang::CharUnits TypeSizeInChars;
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return TypeSizeInChars;
  }
  
  /// \brief Check if this value is zero.
  ///
  /// pre: isCompletelyInitialized() == true
  ///
  virtual bool isZeroImpl() const override {
    auto const ExprTy = Expression->getType();
    auto const CanonTy = ExprTy->getCanonicalTypeUnqualified()->getTypePtr();
    auto const Val = getScalarValueAsAPSInt(FunctionState, CanonTy, LLVMValue);
    
    if (!Val.assigned<llvm::APSInt>())
      return false;
    
    return Val.get<llvm::APSInt>() == 0;
  }
  
public:
  /// \brief Constructor.
  ///
  ValueByRuntimeValueForScalar(::clang::Expr const *ForExpression,
                               seec::trace::FunctionState const &ForState,
                               llvm::Value const *WithLLVMValue,
                               ::clang::CharUnits WithTypeSizeInChars)
  : ValueOfScalar(),
    Expression(ForExpression),
    FunctionState(ForState),
    LLVMValue(WithLLVMValue),
    TypeSizeInChars(WithTypeSizeInChars)
  {}
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return Expression->getType().getCanonicalType().getTypePtr();
  }
  
  /// \brief Get the Expr that this Value is for.
  ///
  virtual ::clang::Expr const *getExpr() const override { return Expression; }
  
  /// \brief Runtime values are never in memory.
  ///
  virtual bool isInMemory() const override { return false; }
  
  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual uintptr_t getAddress() const override { return 0; }
  
  /// \brief Runtime values are always initialized (at the moment).
  ///
  virtual bool isCompletelyInitialized() const override { return true; }
  
  /// \brief Runtime values are never partially initialized (at the moment).
  ///
  virtual bool isPartiallyInitialized() const override { return false; }
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  virtual std::string getValueAsStringShort() const override {
    auto const ExprTy = Expression->getType();
    auto const CanonTy = ExprTy->getCanonicalTypeUnqualified()->getTypePtr();
    return getScalarValueAsString(FunctionState, CanonTy, LLVMValue);
  }
  
  /// \brief Get a string describing the value.
  ///
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
};


//===----------------------------------------------------------------------===//
// ValueByRuntimeValueForPointer
//===----------------------------------------------------------------------===//

/// \brief Represents a pointer Value in LLVM's virtual registers.
///
class ValueByRuntimeValueForPointer final : public ValueOfPointer {
  /// The Store for this Value.
  std::weak_ptr<ValueStore const> Store;
  
  /// The Expr that this value is for.
  ::clang::Expr const *Expression;
  
  /// The MappedAST.
  seec::seec_clang::MappedAST const &MappedAST;
  
  /// The ProcessState that this value is for.
  seec::trace::ProcessState const &ProcessState;
  
  /// The value of this pointer.
  uintptr_t PtrValue;
  
  /// The size of the pointee type.
  ::clang::CharUnits PointeeSize;
  
  /// \brief Constructor.
  ///
  ValueByRuntimeValueForPointer(std::weak_ptr<ValueStore const> WithStore,
                                ::clang::Expr const *ForExpression,
                                seec::seec_clang::MappedAST const &WithAST,
                                seec::trace::ProcessState const &ForState,
                                uintptr_t WithPtrValue,
                                ::clang::CharUnits WithPointeeSize)
  : Store(WithStore),
    Expression(ForExpression),
    MappedAST(WithAST),
    ProcessState(ForState),
    PtrValue(WithPtrValue),
    PointeeSize(WithPointeeSize)
  {}
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return MappedAST.getASTUnit()
                    .getASTContext()
                    .getTypeSizeInChars(Expression->getType());
  }
  
  /// \brief Check if this is a valid opaque pointer (e.g. a DIR *).
  ///
  virtual bool isValidOpaqueImpl() const override {
    return ProcessState.getDir(PtrValue) != nullptr;
  }
  
  /// \brief Get the raw value of this pointer.
  ///
  virtual uintptr_t getRawValueImpl() const override { return PtrValue; }
  
  /// \brief Get the size of the pointee type.
  ///
  virtual ::clang::CharUnits getPointeeSizeImpl() const override {
    return PointeeSize;
  }
  
public:
  /// \brief Attempt ot create a new ValueByRuntimeValueForPointer.
  ///
  static std::shared_ptr<ValueByRuntimeValueForPointer>
  create(std::weak_ptr<ValueStore const> Store,
         seec::seec_clang::MappedStmt const &SMap,
         ::clang::Expr const *Expression,
         seec::trace::FunctionState const &FunctionState,
         llvm::Value const *LLVMValue)
  {
    // Get the raw runtime value of the pointer.
    auto const MaybeValue =
      seec::trace::getCurrentRuntimeValueAs<uintptr_t>
                                           (FunctionState, LLVMValue);
    if (!MaybeValue.assigned())
      return std::shared_ptr<ValueByRuntimeValueForPointer>();
    
    auto const PtrValue = MaybeValue.get<uintptr_t>();
    
    // Get the MappedAST and ASTContext.
    auto const &MappedAST = SMap.getAST();
    auto const &ASTContext = MappedAST.getASTUnit().getASTContext();
    
    // Get the process state.
    auto const &ProcessState = FunctionState.getParent().getParent();
    
    // Get the size of pointee type.
    auto const Type = Expression->getType()->getAs<clang::PointerType>();
    auto const PointeeQType = Type->getPointeeType().getCanonicalType();
    auto const PointeeSize = PointeeQType->isIncompleteType()
                           ? ::clang::CharUnits::fromQuantity(0)
                           : ASTContext.getTypeSizeInChars(PointeeQType);
    
    // Create the object.
    return std::shared_ptr<ValueByRuntimeValueForPointer>
                          (new ValueByRuntimeValueForPointer(Store,
                                                             Expression,
                                                             MappedAST,
                                                             ProcessState,
                                                             PtrValue,
                                                             PointeeSize));
  }
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return Expression->getType().getCanonicalType().getTypePtr();
  }
  
  /// \brief Get the Expr that this Value is for.
  ///
  virtual ::clang::Expr const *getExpr() const override { return Expression; }
  
  /// \brief Runtime values are never in memory.
  ///
  virtual bool isInMemory() const override { return false; }
  
  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual uintptr_t getAddress() const override { return 0; }
  
  /// \brief Runtime values are always initialized (at the moment).
  ///
  virtual bool isCompletelyInitialized() const override { return true; }
  
  /// \brief Runtime values are never partially initialized (at the moment).
  ///
  virtual bool isPartiallyInitialized() const override { return false; }
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  virtual std::string getValueAsStringShort() const override {
    std::string ValueString;
    
    {
      llvm::raw_string_ostream Stream(ValueString);
      Stream << reinterpret_cast<void const *>(PtrValue);
    }
    
    return ValueString;
  }
  
  /// Get a string describing the value.
  ///
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
  
  /// \brief Get the highest legal dereference of this value.
  ///
  virtual unsigned getDereferenceIndexLimit() const override {
    // TODO: Move these calculations into the construction process.
    auto const MaybeArea = ProcessState.getContainingMemoryArea(PtrValue);
    if (!MaybeArea.assigned<MemoryArea>())
      return 0;
    
    if (PointeeSize.isZero())
      return 0;
    
    auto const PointeeTy = Expression->getType()->getPointeeType();
    auto const RecordTy = PointeeTy->getAs< ::clang::RecordType>();
    
    if (RecordTy) {
      auto const RecordDecl = RecordTy->getDecl()->getDefinition();
      if (RecordDecl) {
        if (RecordDecl->hasFlexibleArrayMember()) {
          return 1;
        }
      }
    }
    
    auto const Area = MaybeArea.get<MemoryArea>().withStart(PtrValue);
    return Area.length() / PointeeSize.getQuantity();
  }
  
  /// \brief Get the value of this pointer dereferenced using the given Index.
  ///
  virtual std::shared_ptr<Value const>
  getDereferenced(unsigned Index) const override {
    // Get the store (if it still exists).
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<Value const>();
    
    auto const Address = PtrValue + (Index * PointeeSize.getQuantity());
    
    return getValue(StorePtr,
                    Expression->getType()->getPointeeType(),
                    MappedAST.getASTUnit().getASTContext(),
                    Address,
                    ProcessState);
  }
};


//===----------------------------------------------------------------------===//
// ValueByRuntimeValueForPointerToFILE
//===----------------------------------------------------------------------===//

class ValueByRuntimeValueForPointerToFILE final : public ValueOfPointerToFILE {
  /// The Expr that this value is for.
  ::clang::Expr const *Expression;
  
  /// The raw value of this FILE pointer.
  uintptr_t RawValue;
  
  /// The (unmapped) StreamState for this FILE, or nullptr if it is invalid.
  seec::trace::StreamState const *Stream;
  
  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ::clang::CharUnits::fromQuantity(sizeof(void *));
  }
  
  /// \brief Get the raw value of this pointer.
  ///
  uintptr_t getRawValueImpl() const override { return RawValue; }
  
  /// \brief Check whether this FILE pointer is valid (an open stream).
  ///
  bool isValidImpl() const override { return Stream != nullptr; }
  
  /// \brief Constructor.
  ///
  ValueByRuntimeValueForPointerToFILE(::clang::Expr const *WithExpression,
                                      uintptr_t const WithRawValue,
                                      trace::StreamState const *WithStream)
  : Expression(WithExpression),
    RawValue(WithRawValue),
    Stream(WithStream)
  {}
  
public:
  /// \brief Attempt to create a new \c ValueByRuntimeValueForPointerToFILE.
  ///
  static std::shared_ptr<ValueByRuntimeValueForPointerToFILE>
  create(::clang::Expr const *Expression,
         seec::trace::FunctionState const &FunctionState,
         llvm::Value const *LLVMValue)
  {
    // Get the raw runtime value of the pointer.
    auto const MaybeValue =
      seec::trace::getCurrentRuntimeValueAs<uintptr_t>
                                           (FunctionState, LLVMValue);
    if (!MaybeValue.assigned())
      return std::shared_ptr<ValueByRuntimeValueForPointerToFILE>();
    
    auto const PtrValue = MaybeValue.get<uintptr_t>();
    
    // Check whether the stream is valid.
    auto const &Process = FunctionState.getParent().getParent();
    auto const Stream = Process.getStream(PtrValue);
    
    // Create the object.
    return std::shared_ptr<ValueByRuntimeValueForPointerToFILE>
                          (new ValueByRuntimeValueForPointerToFILE(Expression,
                                                                   PtrValue,
                                                                   Stream));
  }
  
  /// \brief Get the canonical type of this Value.
  ///
  ::clang::Type const *getCanonicalType() const override {
    return Expression->getType().getCanonicalType().getTypePtr();
  }
  
  /// \brief Get the Expr that this Value is for (if any).
  ///
  ::clang::Expr const *getExpr() const override { return Expression; }
  
  /// \brief Check if this represents a value stored in memory.
  ///
  bool isInMemory() const override { return false; }
  
  /// \brief Get the address in memory.
  /// 
  /// pre: isInMemory() == true
  ///
  uintptr_t getAddress() const override { return 0; }
  
  /// \brief Check if this value is completely initialized.
  ///
  bool isCompletelyInitialized() const override { return true; }
  
  /// \brief Check if this value is partially initialized.
  ///
  bool isPartiallyInitialized() const override { return false; }
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  std::string getValueAsStringShort() const override {
    return getValueAsStringFull();
  }
  
  /// \brief Get a string describing the value.
  ///
  std::string getValueAsStringFull() const override {
    if (!Stream)
      return std::string("<invalid FILE>");
    
    std::string Value = "FILE ";
    Value += Stream->getFilename();
    return Value;
  }
};


//===----------------------------------------------------------------------===//
// createValue() from a Type and address.
//===----------------------------------------------------------------------===//

/// \brief Get a Value for a specific type at an address.
///
std::shared_ptr<Value const>
createValue(std::shared_ptr<ValueStore const> Store,
            ::clang::QualType QualType,
            ::clang::ASTContext const &ASTContext,
            uintptr_t Address,
            seec::trace::ProcessState const &ProcessState)
{
  if (!QualType.getTypePtr()) {
    llvm_unreachable("null type");
    return std::shared_ptr<Value const>();
  }
  
  auto const CanonicalType = QualType.getCanonicalType();
  if (CanonicalType->isIncompleteType()) {
    DEBUG(llvm::dbgs() << "Can't create Value for incomplete type.\n");
    return std::shared_ptr<Value const>(); // No values for incomplete types.
  }
  
  auto const TypeSize = ASTContext.getTypeSizeInChars(CanonicalType);
  
  switch (CanonicalType->getTypeClass()) {
    // Scalar values.
    case ::clang::Type::Builtin: SEEC_FALLTHROUGH;
    case ::clang::Type::Atomic:  SEEC_FALLTHROUGH;
    case ::clang::Type::Enum:
    {
      return std::make_shared<ValueByMemoryForScalar>
                             (CanonicalType.getTypePtr(),
                              Address,
                              TypeSize,
                              ProcessState);
    }
    
    case ::clang::Type::Pointer:
    {
      auto const Desugared = QualType.getDesugaredType(ASTContext);
      auto const DesugaredPointer = Desugared->getAs<clang::PointerType>();
      auto const DesugaredPointee = DesugaredPointer->getPointeeType();
      auto const FILEType = ASTContext.getFILEType();
      
      if (DesugaredPointee.getTypePtrOrNull() == FILEType.getTypePtrOrNull())
      {
        return ValueByMemoryForPointerToFILE::create(CanonicalType.getTypePtr(),
                                                     Address,
                                                     ProcessState);
      }
      else {
        return ValueByMemoryForPointer::create(Store,
                                               ASTContext,
                                               CanonicalType.getTypePtr(),
                                               Address,
                                               ProcessState);
      }
    }
    
    case ::clang::Type::Record:
    {
      return ValueByMemoryForRecord::create(Store,
                                            ASTContext,
                                            CanonicalType.getTypePtr(),
                                            Address,
                                            ProcessState);
    }
    
    case ::clang::Type::ConstantArray:   SEEC_FALLTHROUGH;
    case ::clang::Type::IncompleteArray: SEEC_FALLTHROUGH;
    case ::clang::Type::VariableArray:
    {
      return ValueByMemoryForArray::create(Store,
                                           ASTContext,
                                           CanonicalType.getTypePtr(),
                                           Address,
                                           ProcessState);
    }
    
#define SEEC_UNHANDLED_TYPE_CLASS(CLASS)                                       \
    case ::clang::Type::CLASS:                                                 \
      return std::shared_ptr<Value const>();
    
    SEEC_UNHANDLED_TYPE_CLASS(Complex) // TODO.
    
    // Not needed because we don't support the language(s).
    SEEC_UNHANDLED_TYPE_CLASS(BlockPointer) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(LValueReference) // C++
    SEEC_UNHANDLED_TYPE_CLASS(RValueReference) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(MemberPointer) // C++
    SEEC_UNHANDLED_TYPE_CLASS(Auto) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObject) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCInterface) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObjectPointer) //ObjC
    
    SEEC_UNHANDLED_TYPE_CLASS(Vector) // GCC extension
    SEEC_UNHANDLED_TYPE_CLASS(ExtVector) // Extension
    SEEC_UNHANDLED_TYPE_CLASS(FunctionProto)
    SEEC_UNHANDLED_TYPE_CLASS(FunctionNoProto)

    // This function only operates on canonical types, so we
    // automatically ignore non-canonical types, dependent types, and
    // non-canonical-unless-dependent types.
#define TYPE(CLASS, BASE)

#define ABSTRACT_TYPE(CLASS, BASE)

#define NON_CANONICAL_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#define DEPENDENT_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(CLASS, BASE) \
        SEEC_UNHANDLED_TYPE_CLASS(CLASS)

#include "clang/AST/TypeNodes.def"

#undef SEEC_UNHANDLED_TYPE_CLASS
  }
  
  llvm_unreachable("getValue: unhandled type class.");
  return std::shared_ptr<Value const>();
}


//===----------------------------------------------------------------------===//
// ValueStoreImpl
//===----------------------------------------------------------------------===//

class TypedValueSet {
  std::vector<std::pair<MatchType, std::shared_ptr<Value const>>> Items;

public:
  TypedValueSet()
  : Items()
  {}
  
  std::shared_ptr<Value const> getShared(MatchType const &ForType) const {
    for (auto const &Pair : Items)
      if (Pair.first == ForType)
        return Pair.second;
    
    return std::shared_ptr<Value const>{};
  }
  
  Value const *get(MatchType const &ForType) const {
    for (auto const &Pair : Items)
      if (Pair.first == ForType)
        return Pair.second.get();
    
    return nullptr;
  }
  
  void add(MatchType const &ForType, std::shared_ptr<Value const> Val) {
    Items.emplace_back(ForType, std::move(Val));
  }
};

class ValueStoreImpl final {
  /// Control access to the Store variable.
  mutable std::mutex StoreAccess;
  
  // Two-stage lookup to find previously created Value objects.
  // The first stage is the in-memory address of the object.
  // The second stage is the canonical type of the object.
  mutable llvm::DenseMap<uintptr_t, TypedValueSet> Store;
  
  // Disable copying and moving.
  ValueStoreImpl(ValueStoreImpl const &) = delete;
  ValueStoreImpl(ValueStoreImpl &&) = delete;
  ValueStoreImpl &operator=(ValueStoreImpl const &) = delete;
  ValueStoreImpl &operator=(ValueStore &&) = delete;
  
public:
  /// \brief Constructor.
  ValueStoreImpl()
  : Store()
  {}
  
  /// \brief Find or construct a Value for the given type.
  ///
  std::shared_ptr<Value const>
  getValue(std::shared_ptr<ValueStore const> StorePtr,
           ::clang::QualType QualType,
           ::clang::ASTContext const &ASTContext,
           uintptr_t Address,
           seec::trace::ProcessState const &ProcessState) const
  {
    auto const CanonicalType = QualType.getCanonicalType().getTypePtr();
    if (!CanonicalType) {
      DEBUG(llvm::dbgs() << "QualType has no CanonicalType.\n");
      return std::shared_ptr<Value const>();
    }
    
    // Lock the Store.
    std::lock_guard<std::mutex> LockStore(StoreAccess);
    
    // Get (or create) the lookup table for this memory address.
    auto &TypeMap = Store[Address];
    
    auto const Matcher = MatchType(ASTContext, *CanonicalType);
    auto const Existing = TypeMap.getShared(Matcher);
    if (Existing)
      return Existing;
    
    // We must create a new Value.
    auto SharedPtr = createValue(StorePtr,
                                 QualType,
                                 ASTContext,
                                 Address,
                                 ProcessState);
    if (!SharedPtr)
      return SharedPtr;
    
    // Store a shared_ptr for this Value in the lookup table.
    TypeMap.add(Matcher, SharedPtr);
    
    return SharedPtr;
  }
};


//===----------------------------------------------------------------------===//
// ValueStore
//===----------------------------------------------------------------------===//

ValueStore::ValueStore()
: Impl(new ValueStoreImpl())
{}

ValueStore::~ValueStore() = default;

ValueStoreImpl const &ValueStore::getImpl() const {
  return *Impl;
}


//===----------------------------------------------------------------------===//
// getValue() from a type and address.
//===----------------------------------------------------------------------===//

// Documented in MappedValue.hpp
//
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         ::clang::QualType QualType,
         ::clang::ASTContext const &ASTContext,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
{
  return Store->getImpl().getValue(Store,
                                   QualType,
                                   ASTContext,
                                   Address,
                                   ProcessState);
}


//===----------------------------------------------------------------------===//
// getValue() from a mapped ::clang::Stmt.
//===----------------------------------------------------------------------===//

// Documented in MappedValue.hpp
//
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         seec::seec_clang::MappedStmt const &SMap,
         seec::trace::FunctionState const &FunctionState)
{
  auto const Expression = llvm::dyn_cast< ::clang::Expr>(SMap.getStatement());
  if (!Expression)
    return std::shared_ptr<Value const>();
  
  switch (SMap.getMapType()) {
    case seec::seec_clang::MappedStmt::Type::LValSimple:
    {
      // Extract the address of the in-memory object that this lval represents.
      auto const MaybeValue =
        seec::trace::getCurrentRuntimeValueAs<uintptr_t>
                                             (FunctionState, SMap.getValue());
      if (!MaybeValue.assigned()) {
        return std::shared_ptr<Value const>();
      }
      
      auto const PtrValue = MaybeValue.get<uintptr_t>();
      
      // Get the in-memory value at the given address.
      return getValue(Store,
                      Expression->getType(),
                      SMap.getAST().getASTUnit().getASTContext(),
                      PtrValue,
                      FunctionState.getParent().getParent());
    }
    
    case seec::seec_clang::MappedStmt::Type::RValScalar:
    {
      auto const LLVMValues = SMap.getValues();
      if (LLVMValues.first == nullptr) {
        return std::shared_ptr<Value const>();
      }
      
      // If the first Value is an Instruction, then ensure that it has been
      // evaluated and is still valid.
      if (auto const I = llvm::dyn_cast<llvm::Instruction>(LLVMValues.first)) {
        auto const RTV = FunctionState.getCurrentRuntimeValue(I);
        if (!RTV || !RTV->assigned()) {
          return std::shared_ptr<Value const>();
        }
      }
      
      // Simple scalar value.
      if (LLVMValues.second == nullptr) {
        auto const ExprType = Expression->getType();
        
        if (auto const PtrType = ExprType->getAs<clang::PointerType>()) {
          // Pointer types use a special implementation.
          auto const &ASTContext = SMap.getAST().getASTUnit().getASTContext();
          auto const PointeeType = PtrType->getPointeeType();
          auto const FILEType = ASTContext.getFILEType();
          
          if (PointeeType.getTypePtrOrNull() == FILEType.getTypePtrOrNull()) {
            return ValueByRuntimeValueForPointerToFILE
                    ::create(Expression, FunctionState, LLVMValues.first);
          }
          else {
            return ValueByRuntimeValueForPointer::create(Store,
                                                         SMap,
                                                         Expression,
                                                         FunctionState,
                                                         LLVMValues.first);
          }
        }
        else {
          if (ExprType->isIncompleteType())
            return std::shared_ptr<Value const>();
          
          // All other types use a single implementation.
          auto const TypeSize = SMap.getAST()
                                    .getASTUnit()
                                    .getASTContext()
                                    .getTypeSizeInChars(ExprType);
          
          return std::make_shared<ValueByRuntimeValueForScalar>
                                 (Expression,
                                  FunctionState,
                                  LLVMValues.first,
                                  TypeSize);
        }
      }
      
      // Complex value.
      
      // If the second Value is an Instruction, then ensure that it has been
      // evaluated and is still valid.
      if (auto const I = llvm::dyn_cast<llvm::Instruction>(LLVMValues.second)) {
        auto const RTV = FunctionState.getCurrentRuntimeValue(I);
        if (!RTV || !RTV->assigned()) {
          return std::shared_ptr<Value const>();
        }
      }
      
      // TODO: Generate complex Value.
      
      return std::shared_ptr<Value const>();
    }
    
    case seec::seec_clang::MappedStmt::Type::RValAggregate:
    {
      // Extract the address of the in-memory object that this rval represents.
      auto const MaybeValue =
        seec::trace::getCurrentRuntimeValueAs<uintptr_t>
                                             (FunctionState, SMap.getValue());
      if (!MaybeValue.assigned()) {
        return std::shared_ptr<Value const>();
      }
      
      auto const PtrValue = MaybeValue.get<uintptr_t>();
      
      // Get the in-memory value at the given address.
      return getValue(Store,
                      Expression->getType(),
                      SMap.getAST().getASTUnit().getASTContext(),
                      PtrValue,
                      FunctionState.getParent().getParent());
    }
  }
  
  llvm_unreachable("Unhandled MappedStmt::Type!");
  return std::shared_ptr<Value const>();
}


// Documented in MappedValue.hpp
//
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         ::clang::Stmt const *Statement,
         seec::seec_clang::MappedModule const &Mapping,
         seec::trace::FunctionState const &FunctionState)
{
  auto const SMap = Mapping.getMappedStmtForStmt(Statement);
  if (!SMap)
    return std::shared_ptr<Value const>();
  
  return getValue(Store, *SMap, FunctionState);
}


//===----------------------------------------------------------------------===//
// Utilities
//===----------------------------------------------------------------------===//

// Documented in MappedValue.hpp
//
bool isContainedChild(Value const &Child, Value const &Parent)
{
  switch (Parent.getKind()) {
    case Value::Kind::Array:
    {
      auto const &ParentArray = static_cast<ValueOfArray const &>(Parent);
      auto const Limit = ParentArray.getChildCount();
      
      for (unsigned i = 0; i < Limit; ++i) {
        auto const Element = ParentArray.getChildAt(i);
        if (&Child == Element.get() || isContainedChild(Child, *Element))
          return true;
      }
      
      return false;
    }
    
    case Value::Kind::Record:
    {
      auto const &ParentRecord = static_cast<ValueOfRecord const &>(Parent);
      auto const Limit = ParentRecord.getChildCount();
      
      for (unsigned i = 0; i < Limit; ++i) {
        auto const Member = ParentRecord.getChildAt(i);
        if (&Child == Member.get() || isContainedChild(Child, *Member))
          return true;
      }
      
      return false;
    }
    
    case Value::Kind::Basic: SEEC_FALLTHROUGH;
    case Value::Kind::Scalar: SEEC_FALLTHROUGH;
    case Value::Kind::Pointer: SEEC_FALLTHROUGH;
    case Value::Kind::PointerToFILE:
      return false;
  }
  
  return false;
}

// Documented in MappedValue.hpp
//
bool doReferenceSameValue(ValueOfPointer const &LHS, ValueOfPointer const &RHS)
{
  // Ensure that both pointers reference at least one Value.
  auto const LLim = LHS.getDereferenceIndexLimit();
  auto const RLim = RHS.getDereferenceIndexLimit();
  
  if (LLim == 0 || RLim == 0)
    return false;
  
  auto const L0 = LHS.getDereferenced(0);
  auto const R0 = RHS.getDereferenced(0);
  
  // Fail quickly if the pointees have different types.
  if (L0->getCanonicalType() != R0->getCanonicalType())
    return false;
  
  if (LHS.getRawValue() <= RHS.getRawValue()) {
    auto const Limit = LHS.getDereferenceIndexLimit();
    
    for (unsigned i = 0; i < Limit; ++i)
      if (LHS.getDereferenced(i) == R0)
        return true;
  }
  else {
    auto const Limit = RHS.getDereferenceIndexLimit();
    
    for (unsigned i = 0; i < Limit; ++i)
      if (RHS.getDereferenced(i) == L0)
        return true;
  }
  
  return false;
}


} // namespace cm (in seec)

} // namespace seec
