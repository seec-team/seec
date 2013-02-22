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

#include "seec/Clang/MappedAST.hpp"
#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedStmt.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Trace/ProcessState.hpp"
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

#include "llvm/Support/raw_ostream.h"


namespace seec {

namespace cm {


//===----------------------------------------------------------------------===//
// ValueStoreImpl
//===----------------------------------------------------------------------===//

class ValueStoreImpl {

public:
  /// \brief Constructor.
  ValueStoreImpl()
  {}
  
  /// \brief Free up unused memory.
  ///
  void freeUnused() const {}
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

void ValueStore::freeUnused() const {
  Impl->freeUnused();
}


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
    
    std::string RetStr;
    
    {
      llvm::raw_string_ostream Stream(RetStr);
      auto const Bytes = Region.getByteValues();
      Stream << *reinterpret_cast<T const *>(Bytes.data());
    }
    
    return RetStr;
  }
};

template<>
struct GetMemoryOfBuiltinAsString<long double> {
  static std::string impl(seec::trace::MemoryState::Region const &Region) {
    return std::string("<long double: not implemented>");
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

// Forward declaration.
std::string
getScalarValueAsString(::clang::QualType QualType,
                       seec::trace::MemoryState::Region const &Region);

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
      return getScalarValueAsString(Ty->getValueType(), Region);
    }
    
    // EnumType
    case ::clang::Type::Enum:
    {
      // Recursive on the underlying type.
      auto const Ty = llvm::cast< ::clang::EnumType>(CanonType);
      return getScalarValueAsString(Ty->getDecl()->getIntegerType(), Region);
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

std::string
getScalarValueAsString(::clang::QualType QualType,
                       seec::trace::MemoryState::Region const &Region)
{
  return getScalarValueAsString(QualType.getTypePtr(), Region);
}


//===----------------------------------------------------------------------===//
// ValueByMemoryForScalar
//===----------------------------------------------------------------------===//

/// \brief Represents a simple scalar Value in memory.
///
class ValueByMemoryForScalar : public Value {
  ::clang::QualType QualType;
  
  uintptr_t Address;
  
  ::clang::CharUnits Size;
  
  seec::trace::MemoryState const &Memory;
  
public:
  /// \brief Constructor.
  ///
  ValueByMemoryForScalar(::clang::QualType WithQualType,
                         uintptr_t WithAddress,
                         ::clang::CharUnits WithSize,
                         seec::trace::ProcessState const &ForProcessState)
  : QualType(WithQualType),
    Address(WithAddress),
    Size(WithSize),
    Memory(ForProcessState.getMemory())
  {}
  
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  virtual bool isInMemory() const override { return true; }
  
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
    
    return getScalarValueAsString(QualType,
                                  Memory.getRegion(MemoryArea(Address,
                                                              Length)));
  }
  
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
  
  virtual unsigned getChildCount() const override { return 0; }
  
  virtual std::shared_ptr<Value const>
  getChildAt(unsigned Index) const override {
    return std::shared_ptr<Value const>();
  }
  
  virtual unsigned getDereferenceIndexLimit() const override { return 0; }
  
  virtual std::shared_ptr<Value const>
  getDereferenced(unsigned Index) const override {
    return std::shared_ptr<Value const>();
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForPointer - TODO
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// ValueByMemoryForRecord
//===----------------------------------------------------------------------===//

/// \brief Represents a record Value in memory.
///
class ValueByMemoryForRecord : public Value {
  /// The Context for this Value.
  ::clang::ASTContext const &ASTContext;
  
  /// The layout information for this Record.
  ::clang::ASTRecordLayout const &Layout;
  
  /// The type of this Value.
  ::clang::QualType QualType;
  
  /// The memory address of this Value.
  uintptr_t Address;
  
  /// The process state that this Value is in.
  seec::trace::ProcessState const &ProcessState;
  
  /// \brief Constructor.
  ///
  ValueByMemoryForRecord(::clang::ASTContext const &WithASTContext,
                         ::clang::ASTRecordLayout const &WithLayout,
                         ::clang::QualType WithQualType,
                         uintptr_t WithAddress,
                         seec::trace::ProcessState const &ForProcessState)
  : ASTContext(WithASTContext),
    Layout(WithLayout),
    QualType(WithQualType),
    Address(WithAddress),
    ProcessState(ForProcessState)
  {}
  
public:
  /// \brief Attempt to create a new instance of this class.
  ///
  static std::shared_ptr<ValueByMemoryForRecord>
  create(::clang::ASTContext const &ASTContext,
         ::clang::QualType QualType,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
  {
    auto const CanonTy = QualType.getCanonicalType().getTypePtr();
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonTy);
    auto const Decl = RecordTy->getDecl()->getDefinition();
    if (!Decl)
      return std::shared_ptr<ValueByMemoryForRecord>();
    
    auto const &Layout = ASTContext.getASTRecordLayout(Decl);
    
    return std::shared_ptr<ValueByMemoryForRecord>
                          (new ValueByMemoryForRecord(ASTContext,
                                                      Layout,
                                                      QualType,
                                                      Address,
                                                      ProcessState));
  }
  
  /// \brief In-memory values are never for an Expr.
  /// \return nullptr.
  ///
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  /// \brief In-memory values are always in memory.
  /// \return true.
  ///
  virtual bool isInMemory() const override { return true; }
  
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
    auto const CanonTy = QualType.getCanonicalType().getTypePtr();
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonTy);
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
  
  /// \brief Get the Value of a member of this record.
  ///
  virtual std::shared_ptr<Value const>
  getChildAt(unsigned Index) const override {
    assert(Index < getChildCount() && "Invalid Child Index");
    
    auto const CanonTy = QualType.getCanonicalType().getTypePtr();
    auto const RecordTy = llvm::cast< ::clang::RecordType>(CanonTy);
    auto const Decl = RecordTy->getDecl()->getDefinition();
    
    auto FieldIt = Decl->field_begin();
    for (auto FieldEnd = Decl->field_end(); ; ++FieldIt) {
      if (FieldIt == FieldEnd)
        return std::shared_ptr<Value const>();
      
      if (FieldIt->getFieldIndex() == Index)
        break;
    }
    
    // We don't support bitfields yet!
    auto const BitOffset = Layout.getFieldOffset(Index);
    if (BitOffset % CHAR_BIT != 0)
      return std::shared_ptr<Value const>();
    
    return getValue(FieldIt->getType(),
                    ASTContext,
                    Address + (BitOffset / CHAR_BIT),
                    ProcessState);
  }
  
  /// \brief Records are never dereferenced.
  ///
  virtual unsigned getDereferenceIndexLimit() const override { return 0; }
  
  /// \brief Records are never dereferenced.
  ///
  virtual std::shared_ptr<Value const>
  getDereferenced(unsigned Index) const override {
    return std::shared_ptr<Value const>();
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForArray
//===----------------------------------------------------------------------===//

/// \brief Represents an array Value in memory.
///
class ValueByMemoryForArray : public Value {
  /// The Context for this Value.
  ::clang::ASTContext const &ASTContext;
  
  /// The type of this Value.
  ::clang::QualType QualType;
  
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
  ValueByMemoryForArray(::clang::ASTContext const &WithASTContext,
                        ::clang::QualType WithQualType,
                        uintptr_t WithAddress,
                        unsigned WithElementSize,
                        unsigned WithElementCount,
                        seec::trace::ProcessState const &ForProcessState)
  : ASTContext(WithASTContext),
    QualType(WithQualType),
    Address(WithAddress),
    ElementSize(WithElementSize),
    ElementCount(WithElementCount),
    ProcessState(ForProcessState)
  {}
  
public:
  /// \brief Attempt to create a new instance of this class.
  ///
  static std::shared_ptr<ValueByMemoryForArray const>
  create(::clang::ASTContext const &ASTContext,
         ::clang::QualType QualType,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
  {
    auto const CanonTy = QualType.getCanonicalType().getTypePtr();
    auto const ArrayTy = llvm::cast< ::clang::ArrayType>(CanonTy);
    auto const ElementTy = ArrayTy->getElementType();
    auto const ElementSize = ASTContext.getTypeSizeInChars(ElementTy);
    
    if (ElementSize.isZero()) {
      llvm::errs() << "Array's element type has size zero.\n";
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
                          (new ValueByMemoryForArray(ASTContext,
                                                     QualType,
                                                     Address,
                                                     ElementSize.getQuantity(),
                                                     ElementCount,
                                                     ProcessState));
  }
  
  /// \brief In-memory values are never for an Expr.
  /// \return nullptr.
  ///
  virtual ::clang::Expr const *getExpr() const override { return nullptr; }
  
  /// \brief In-memory values are always in memory.
  /// \return true.
  ///
  virtual bool isInMemory() const override { return true; }
  
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
    
    auto const ArrayTy = QualType->getAsArrayTypeUnsafe();
    assert(ArrayTy);
    
    auto const ChildAddress = Address + (Index * ElementSize);
    
    return getValue(ArrayTy->getElementType(),
                    ASTContext,
                    ChildAddress,
                    ProcessState);
  }
  
  /// \brief Records are never dereferenced.
  ///
  virtual unsigned getDereferenceIndexLimit() const override { return 0; }
  
  /// \brief Records are never dereferenced.
  ///
  virtual std::shared_ptr<Value const>
  getDereferenced(unsigned Index) const override {
    return std::shared_ptr<Value const>();
  }
};


//===----------------------------------------------------------------------===//
// getScalarValueAsAPSInt() - from llvm::Value
//===----------------------------------------------------------------------===//

template<typename T>
struct GetValueOfBuiltinAsAPSInt {
  static seec::util::Maybe<llvm::APSInt>
  impl(seec::trace::FunctionState const &State, ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getCurrentRuntimeValueAs<T>
                                                                 (State, Value);
    if (!MaybeValue.assigned())
      return seec::util::Maybe<llvm::APSInt>();
    
    llvm::APSInt APSValue(sizeof(T) * CHAR_BIT, std::is_unsigned<T>::value);
    APSValue = MaybeValue.template get<0>();
    
    return APSValue;
  }
};

seec::util::Maybe<llvm::APSInt>
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
      return seec::util::Maybe<llvm::APSInt>();

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
}

seec::util::Maybe<llvm::APSInt>
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
      return seec::util::Maybe<llvm::APSInt>();
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
    if (!MaybeValue.assigned())
      return std::string("<builtin T: couldn't get current runtime value>");
    
    std::string RetStr;
    
    {
      llvm::raw_string_ostream Stream(RetStr);
      Stream << MaybeValue.template get<0>();
    }
    
    return RetStr;
  }
};

template<>
struct GetValueOfBuiltinAsString<long double> {
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getCurrentRuntimeValueAs<long double>
                                                                 (State, Value);
    if (!MaybeValue.assigned())
      return std::string("<long double: couldn't get current runtime value>");
    
    auto const LDValue = MaybeValue.get<0>();
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
}


//===----------------------------------------------------------------------===//
// ValueByRuntimeValue
//===----------------------------------------------------------------------===//

/// \brief Represents a Value in LLVM's virtual registers.
///
class ValueByRuntimeValue : public Value {
public:
  /// \brief Constructor.
  ///
  ValueByRuntimeValue()
  {}
  
  /// \brief Virtual destructor required.
  ///
  virtual ~ValueByRuntimeValue() = default;
  
  /// \brief Runtime values are never in memory.
  ///
  virtual bool isInMemory() const override { return false; }
  
  /// \brief Runtime values are always initialized (at the moment).
  ///
  virtual bool isCompletelyInitialized() const override { return true; }
  
  /// \brief Runtime values are never partially initialized (at the moment).
  ///
  virtual bool isPartiallyInitialized() const override { return false; }
};


//===----------------------------------------------------------------------===//
// ValueByRuntimeValueForScalar
//===----------------------------------------------------------------------===//

/// \brief Represents a simple scalar Value in LLVM's virtual registers.
///
class ValueByRuntimeValueForScalar : public ValueByRuntimeValue {
  /// The Expr that this value is for.
  ::clang::Expr const *Expression;
  
  /// The FunctionState that this value is for.
  seec::trace::FunctionState const &FunctionState;
  
  /// The LLVM Value for this value.
  llvm::Value const *LLVMValue;
  
public:
  /// \brief Constructor.
  ///
  ValueByRuntimeValueForScalar(::clang::Expr const *ForExpression,
                               seec::trace::FunctionState const &ForState,
                               llvm::Value const *WithLLVMValue)
  : Expression(ForExpression),
    FunctionState(ForState),
    LLVMValue(WithLLVMValue)
  {}
  
  /// \brief Virtual destructor required.
  ///
  virtual ~ValueByRuntimeValueForScalar() = default;
  
  /// \brief Get the Expr that this Value is for.
  ///
  virtual ::clang::Expr const *getExpr() const override { return Expression; }
  
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
  
  /// \brief Scalar values have no children.
  /// \return Always 0.
  ///
  virtual unsigned getChildCount() const override { return 0; }
  
  /// \brief Scalar values have no children.
  /// \return An empty std::shared_ptr<Value const>
  ///
  virtual std::shared_ptr<Value const>
  getChildAt(unsigned Index) const override {
    return std::shared_ptr<Value const>();
  }
  
  /// \brief Scalar values do not dereference.
  ///
  virtual unsigned getDereferenceIndexLimit() const override { return 0; }
  
  /// \brief Scalar values do not dereference.
  ///
  /// Pointers are implemented using ValueByRuntimeValueForPointer.
  ///
  virtual std::shared_ptr<Value const>
  getDereferenced(unsigned Index) const override {
    return std::shared_ptr<Value const>();
  }
};


//===----------------------------------------------------------------------===//
// ValueByRuntimeValueForPointer
//===----------------------------------------------------------------------===//

/// \brief Represents a pointer Value in LLVM's virtual registers.
///
class ValueByRuntimeValueForPointer : public ValueByRuntimeValue {
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
  ValueByRuntimeValueForPointer(::clang::Expr const *ForExpression,
                                seec::seec_clang::MappedAST const &WithAST,
                                seec::trace::ProcessState const &ForState,
                                uintptr_t WithPtrValue,
                                ::clang::CharUnits WithPointeeSize)
  : Expression(ForExpression),
    MappedAST(WithAST),
    ProcessState(ForState),
    PtrValue(WithPtrValue),
    PointeeSize(WithPointeeSize)
  {}
  
public:
  /// \brief Attempt ot create a new ValueByRuntimeValueForPointer.
  ///
  static std::shared_ptr<ValueByRuntimeValueForPointer>
  create(seec::seec_clang::MappedStmt const &SMap,
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
    auto const Type = llvm::cast< ::clang::PointerType>
                                (Expression->getType().getTypePtr());
    auto const PointeeQType = Type->getPointeeType();
    auto const PointeeSize = ASTContext.getTypeSizeInChars(PointeeQType);
    
    // Create the object.
    return std::shared_ptr<ValueByRuntimeValueForPointer>
                          (new ValueByRuntimeValueForPointer(Expression,
                                                             MappedAST,
                                                             ProcessState,
                                                             PtrValue,
                                                             PointeeSize));
  }
  
  /// \brief Virtual destructor required.
  ///
  virtual ~ValueByRuntimeValueForPointer() = default;
  
  /// \brief Get the Expr that this Value is for.
  ///
  virtual ::clang::Expr const *getExpr() const override { return Expression; }
  
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
  
  /// \brief Pointers have no children.
  /// \return Always 0.
  ///
  virtual unsigned getChildCount() const override { return 0; }
  
  /// \brief Pointers have no children.
  /// \return An empty std::shared_ptr<Value const>
  ///
  virtual std::shared_ptr<Value const>
  getChildAt(unsigned Index) const override {
    return std::shared_ptr<Value const>();
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
      llvm::errs() << "pointee is record.\n";
      
      auto const RecordDecl = RecordTy->getDecl()->getDefinition();
      if (RecordDecl) {
        llvm::errs() << "got pointee definition.\n";
        
        if (RecordDecl->hasFlexibleArrayMember()) {
          llvm::errs() << "pointee has flexible array member.\n";
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
    auto const Address = PtrValue + (Index * PointeeSize.getQuantity());
    
    return getValue(Expression->getType()->getPointeeType(),
                    MappedAST.getASTUnit().getASTContext(),
                    Address,
                    ProcessState);
  }
};


//===----------------------------------------------------------------------===//
// getValue()
//===----------------------------------------------------------------------===//


/// \brief Get a Value for a specific type at an address.
///
std::shared_ptr<Value const>
getValue(::clang::QualType QualType,
         ::clang::ASTContext const &ASTContext,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState)
{
  if (!QualType.getTypePtr()) {
    llvm::errs() << "null type.\n";
    return std::shared_ptr<Value const>();
  }
  
  auto const CanonicalType = QualType.getCanonicalType();
  auto const TypeSize = ASTContext.getTypeSizeInChars(CanonicalType);
  
  switch (CanonicalType->getTypeClass()) {
    // Scalar values.
    case ::clang::Type::Builtin: SEEC_FALLTHROUGH;
    case ::clang::Type::Atomic:  SEEC_FALLTHROUGH;
    case ::clang::Type::Enum:    SEEC_FALLTHROUGH;
    case ::clang::Type::Pointer: SEEC_FALLTHROUGH;
    {
      return std::make_shared<ValueByMemoryForScalar>
                             (QualType, Address, TypeSize, ProcessState);
    }
    
    case ::clang::Type::Record:
    {
      return ValueByMemoryForRecord::create(ASTContext,
                                            QualType,
                                            Address,
                                            ProcessState);
    }
    
    case ::clang::Type::ConstantArray:   SEEC_FALLTHROUGH;
    case ::clang::Type::IncompleteArray: SEEC_FALLTHROUGH;
    case ::clang::Type::VariableArray:
    {
      return ValueByMemoryForArray::create(ASTContext,
                                           QualType,
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


// Documented in MappedValue.hpp
//
std::shared_ptr<Value const>
getValue(seec::seec_clang::MappedStmt const &SMap,
         seec::trace::FunctionState const &FunctionState)
{
  auto const Expression = llvm::dyn_cast< ::clang::Expr>(SMap.getStatement());
  if (!Expression)
    return std::shared_ptr<Value const>();
  
  switch (SMap.getMapType()) {
    case seec::seec_clang::MappedStmt::Type::LValSimple:
    {
      llvm::errs() << "Creating LValSimple.\n";
      
      // Extract the address of the in-memory object that this lval represents.
      auto const MaybeValue =
        seec::trace::getCurrentRuntimeValueAs<uintptr_t>
                                             (FunctionState, SMap.getValue());
      if (!MaybeValue.assigned()) {
        llvm::errs() << "No address assigned.\n";
        return std::shared_ptr<Value const>();
      }
      
      auto const PtrValue = MaybeValue.get<uintptr_t>();
      
      // Get the in-memory value at the given address.
      return getValue(Expression->getType(),
                      SMap.getAST().getASTUnit().getASTContext(),
                      PtrValue,
                      FunctionState.getParent().getParent());
    }
    
    case seec::seec_clang::MappedStmt::Type::RValScalar:
    {
      auto const LLVMValues = SMap.getValues();
      if (LLVMValues.first == nullptr) {
        llvm::errs() << "Mapped llvm::Value is NULL.\n";
        return std::shared_ptr<Value const>();
      }
      
      // Ensure that an Instruction has produced a value.
      if (auto const I = llvm::dyn_cast<llvm::Instruction>(LLVMValues.first)) {
        if (auto const RTV = FunctionState.getCurrentRuntimeValue(I)) {
          if (!RTV->assigned()) {
            llvm::errs() << "Mapped llvm::Instruction not yet assigned.\n";
            return std::shared_ptr<Value const>();
          }
          
          // TODO: Ensure that Instruction value is still valid.
        }
      }
      
      // Simple scalar value.
      if (LLVMValues.second == nullptr) {
        if (Expression->getType()->isPointerType()) {
          // Pointer types use a special implementation.
          return ValueByRuntimeValueForPointer::create(SMap,
                                                       Expression,
                                                       FunctionState,
                                                       LLVMValues.first);
        }
        else {
          // All other types use a single implementation.
          return std::make_shared<ValueByRuntimeValueForScalar>
                                 (Expression,
                                  FunctionState,
                                  LLVMValues.first);
        }
      }
      
      // Complex value.
      
      // Ensure that an Instruction has produced a value.
      if (auto const I = llvm::dyn_cast<llvm::Instruction>(LLVMValues.second)) {
        if (auto const RTV = FunctionState.getCurrentRuntimeValue(I)) {
          if (!RTV->assigned()) {
            llvm::errs() << "Mapped llvm::Instruction not yet assigned.\n";
            return std::shared_ptr<Value const>();
          }
          
          // TODO: Ensure that Instruction value is still valid.
        }
      }
      
      // TODO.
      llvm::errs() << "Mapped complex values not supported.\n";
      return std::shared_ptr<Value const>();
    }
    
    case seec::seec_clang::MappedStmt::Type::RValAggregate:
    {
      llvm::errs() << "Creating RValAggregate.\n";
      
      // Extract the address of the in-memory object that this rval represents.
      auto const MaybeValue =
        seec::trace::getCurrentRuntimeValueAs<uintptr_t>
                                             (FunctionState, SMap.getValue());
      if (!MaybeValue.assigned()) {
        llvm::errs() << "No address assigned.\n";
        return std::shared_ptr<Value const>();
      }
      
      auto const PtrValue = MaybeValue.get<uintptr_t>();
      
      // Get the in-memory value at the given address.
      return getValue(Expression->getType(),
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
getValue(::clang::Stmt const *Statement,
         seec::seec_clang::MappedModule const &Mapping,
         seec::trace::FunctionState const &FunctionState)
{
  auto const SMap = Mapping.getMappedStmtForStmt(Statement);
  if (!SMap)
    return std::shared_ptr<Value const>();
  
  return getValue(*SMap, FunctionState);
}


} // namespace cm (in seec)

} // namespace seec
