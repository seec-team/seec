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
#include "seec/Trace/GetRecreatedValue.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/Maybe.hpp"
#include "seec/Util/Range.hpp"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/ASTUnit.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"

#include <cctype>
#include <string>


using namespace std;


namespace seec {

namespace cm {


/// \brief Used to store the initialization state of a Value.
///
enum class InitializationState {
  None,
  Partial,
  Complete
};

std::shared_ptr<Value const>
getValueImpl(std::shared_ptr<ValueStore const> Store,
             seec::seec_clang::MappedStmt const &SMap,
             seec::trace::FunctionState const &FunctionState);


//===----------------------------------------------------------------------===//
// Value
//===----------------------------------------------------------------------===//

Value::~Value() = default;


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

  std::shared_ptr<Value const> getSharedFromTypeString(llvm::StringRef TS) const
  {
    for (auto const &Pair : Items)
      if (Pair.second->getTypeAsString() == TS)
        return Pair.second;

    return std::shared_ptr<Value const>();
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

struct StatementValueKey
: std::pair<seec::seec_clang::MappedStmt const * const,
            seec::trace::FunctionState const * const>
{
  StatementValueKey(seec::seec_clang::MappedStmt const *Stmt,
                    seec::trace::FunctionState const *Func)
  : std::pair<seec::seec_clang::MappedStmt const * const,
              seec::trace::FunctionState const * const>(Stmt, Func)
  {}
};

class ValueStoreImpl final {
  /// Control access to the Store variable.
  mutable std::mutex StoreAccess;

  // Two-stage lookup to find previously created Value objects.
  // The first stage is the in-memory address of the object.
  // The second stage is the canonical type of the object.
  mutable llvm::DenseMap<stateptr_ty, TypedValueSet> Store;
  
  mutable std::mutex m_StmtValuesMutex;
  mutable std::map<StatementValueKey, std::shared_ptr<Value const>>
    m_StmtValues;

  /// SeeC-Clang mapping information.
  seec::seec_clang::MappedModule const &Mapping;

  // Disable copying and moving.
  ValueStoreImpl(ValueStoreImpl const &) = delete;
  ValueStoreImpl(ValueStoreImpl &&) = delete;
  ValueStoreImpl &operator=(ValueStoreImpl const &) = delete;
  ValueStoreImpl &operator=(ValueStore &&) = delete;

public:
  /// \brief Constructor.
  ValueStoreImpl(seec::seec_clang::MappedModule const &WithMapping)
  : StoreAccess(),
    Store(),
    m_StmtValuesMutex(),
    m_StmtValues(),
    Mapping(WithMapping)
  {}

  /// \brief Find or construct a Value for the given type.
  ///
  std::shared_ptr<Value const>
  getValue(std::shared_ptr<ValueStore const> StorePtr,
           ::clang::QualType QualType,
           ::clang::ASTContext const &ASTContext,
           stateptr_ty Address,
           seec::trace::ProcessState const &ProcessState,
           seec::trace::FunctionState const *OwningFunction) const;

  /// \brief Find or construct a Value from a runtime value.
  ///
  std::shared_ptr<Value const>
  getValue(std::shared_ptr<ValueStore const> Store,
           seec::seec_clang::MappedStmt const &SMap,
           seec::trace::FunctionState const &FunctionState) const
  {
    std::lock_guard<std::mutex> Lock(m_StmtValuesMutex);
    
    auto &Value = m_StmtValues[StatementValueKey{&SMap, &FunctionState}];
    
    if (!Value) {
      Value = getValueImpl(Store, SMap, FunctionState);
    }
    
    return Value;
  }
  
  /// \brief Get SeeC-Clang mapping information.
  ///
  seec::seec_clang::MappedModule const &getMapping() const { return Mapping; }

  /// \brief Find first \c Value matching the given predicate.
  ///
  std::shared_ptr<Value const>
  findFromAddressAndType(stateptr_ty Address, llvm::StringRef TypeString) const
  {
    auto const It = Store.find(Address);

    if (It == Store.end())
      return std::shared_ptr<Value const>();

    return It->second.getSharedFromTypeString(TypeString);
  }
};


//===----------------------------------------------------------------------===//
// readAPIntFromMemory()
//===----------------------------------------------------------------------===//

Maybe<llvm::APInt> readAPIntFromMemory(clang::ASTContext const &AST,
                                       clang::Type const *Type,
                                       stateptr_ty const Address,
                                       seec::trace::MemoryState const &Memory)
{
  auto const Size = AST.getTypeSizeInChars(Type);
  auto const Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
  if (!Region.isAllocated() || !Region.isCompletelyInitialized())
    return Maybe<llvm::APInt>();

  auto const BitWidth = AST.getTypeSize(Type);
  auto const RawBytes = Region.getByteValues();
  auto const Data = RawBytes.data();

  if (AST.getTargetInfo().isBigEndian() == llvm::sys::IsBigEndianHost) {
    switch (BitWidth) {
    case 8:  return llvm::APInt(8,  *reinterpret_cast<uint8_t  const *>(Data));
    case 16: return llvm::APInt(16, *reinterpret_cast<uint16_t const *>(Data));
    case 32: return llvm::APInt(32, *reinterpret_cast<uint32_t const *>(Data));
    case 64: return llvm::APInt(64, *reinterpret_cast<uint64_t const *>(Data));
    }
  }
  else {
    switch (BitWidth) {
    case 8:  return llvm::APInt(8,  *reinterpret_cast<uint8_t  const *>(Data));
    case 16: return llvm::APInt(16, *reinterpret_cast<uint16_t const *>(Data))
                          .byteSwap();
    case 32: return llvm::APInt(32, *reinterpret_cast<uint32_t const *>(Data))
                          .byteSwap();
    case 64: return llvm::APInt(64, *reinterpret_cast<uint64_t const *>(Data))
                          .byteSwap();
    }
  }

  llvm::errs() << "readAPIntFromMemory: unsupported bitwidth " << BitWidth
               << "\n";
  return Maybe<llvm::APInt>();
}


//===----------------------------------------------------------------------===//
// getScalarValueAsString() - from memory
//===----------------------------------------------------------------------===//

std::string
getScalarValueAsString(clang::ASTContext const &AST,
                       clang::BuiltinType const *Type,
                       stateptr_ty const Address,
                       seec::trace::MemoryState const &Memory)
{
  if (Type->getKind() == clang::BuiltinType::Char_S
      || Type->getKind() == clang::BuiltinType::Char_U)
  {
    // Special handling to pretty-print chars.
    auto const Region = Memory.getRegion(MemoryArea(Address, 1));
    if (!Region.isAllocated() || !Region.isCompletelyInitialized())
      return std::string{"<uninitialized>"};

    auto const Value = Region.getByteValues()[0];
    std::string Printed;

    if ((static_cast<uint8_t>(Value) & 128) == 0) {
      static char const * const FormattedASCII[] = {
        "\\0", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",  "BS", "\\t",
        "\\n",  "VT", "\\f", "\\r",  "SO",  "SI", "DLE", "DC1", "DC2", "DC3",
        "DC4", "NAK", "SYN", "ETB", "CAN",  "EM", "SUB", "ESC",  "FS",  "GS",
         "RS",  "US",   " ",   "!",  "\"",   "#",   "$",   "%",   "&",   "'",
          "(",   ")",   "*",   "+",   ",",   "-",   ".",   "/",   "0",   "1",
          "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",   ":",   ";",
          "<",   "=",   ">",   "?",   "@",   "A",   "B",   "C",   "D",   "E",
          "F",   "G",   "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
          "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",   "X",   "Y",
          "Z",   "[",  "\\",   "]",   "^",   "_",   "`",   "a",   "b",   "c",
          "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",   "m",
          "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
          "x",   "y",   "z",   "{",   "|",   "}",   "~", "DEL" };

      Printed = FormattedASCII[static_cast<unsigned>(Value)];
    }
    else if (Type->getKind() == clang::BuiltinType::Char_S) {
      Printed = std::to_string(static_cast<signed char>(Value));
    }
    else {
      Printed = std::to_string(static_cast<unsigned char>(Value));
    }

    return Printed;
  }
  else if (Type->isInteger()) {
    auto const Size     = AST.getTypeSizeInChars(Type);
    auto const Region   = Memory.getRegion(MemoryArea(Address,
                                                      Size.getQuantity()));

    if (!Region.isAllocated() || !Region.isCompletelyInitialized())
      return std::string{"<unallocated or uninitialized>"};

    auto const BitWidth = AST.getTypeSize(Type);
    auto const NWords   = BitWidth / 64 + (BitWidth % 64 ? 1 : 0);

    uint64_t Words[NWords];
    std::memcpy(reinterpret_cast<char *>(Words),
                Region.getByteValues().data(),
                Size.getQuantity());

    llvm::SmallString<32> Buffer;

    if (AST.getTargetInfo().isBigEndian() == llvm::sys::IsBigEndianHost) {
      // Host and recorder have the same endianness.
      llvm::APSInt Value(llvm::APInt(BitWidth,
                                     llvm::ArrayRef<uint64_t>(Words, NWords)),
                         Type->isUnsignedInteger());
      Value.toString(Buffer);
    }
    else {
      // Recorder's representation must be byte-swapped to match current host.
      llvm::APSInt Value(llvm::APInt(BitWidth,
                                     llvm::ArrayRef<uint64_t>(Words, NWords))
                                    .byteSwap(),
                         Type->isUnsignedInteger());
      Value.toString(Buffer);
    }

    return Buffer.str().str();
  }
  else if (Type->isFloatingPoint()) {
    auto const &Semantics = AST.getFloatTypeSemantics(clang::QualType(Type, 0));

    if (&Semantics == &llvm::APFloat::IEEEsingle()) {
      auto const Region = Memory.getRegion(MemoryArea(Address, sizeof(float)));
      if (!Region.isAllocated() || !Region.isCompletelyInitialized())
        return std::string{"<unallocated or uninitialized>"};

      auto const RawBytes = Region.getByteValues();
      auto const Value = *reinterpret_cast<float const *>(RawBytes.data());
      return std::to_string(Value);
    }
    else if (&Semantics == &llvm::APFloat::IEEEdouble()) {
      auto const Region = Memory.getRegion(MemoryArea(Address, sizeof(double)));
      if (!Region.isAllocated() || !Region.isCompletelyInitialized())
        return std::string{"<unallocated or uninitialized>"};

      auto const RawBytes = Region.getByteValues();
      auto const Value = *reinterpret_cast<double const *>(RawBytes.data());
      return std::to_string(Value);
    }
    else if (&Semantics == &llvm::APFloat::x87DoubleExtended()) {
      auto const Region = Memory.getRegion(MemoryArea(Address, 10));
      if (!Region.isAllocated() || !Region.isCompletelyInitialized())
        return std::string{"<unallocated or uninitialized>"};

      uint64_t Vals[2] = {0, 0}; // 16 bytes.
      memcpy(reinterpret_cast<char *>(Vals), Region.getByteValues().data(), 10);
      llvm::APFloat APF(Semantics, llvm::APInt(80, Vals));

      llvm::SmallString<32> Buffer;
      APF.toString(Buffer);
      return Buffer.str().str();
    }
    else if (&Semantics == &llvm::APFloat::IEEEhalf()) {
      return std::string{"<IEEEhalf unsupported>"};
    }
    else if (&Semantics == &llvm::APFloat::IEEEquad()) {
      return std::string{"<IEEEquad unsupported>"};
    }
    else if (&Semantics == &llvm::APFloat::PPCDoubleDouble()) {
      return std::string{"<PPCDoubleDouble unsupported>"};
    }
  }
  else if (Type->isVoidType()) {
    return std::string{"<void>"};
  }

  clang::LangOptions LangOpts;
  clang::PrintingPolicy Policy(LangOpts);
  llvm::errs() << "unexpected builtin: " << Type->getName(Policy) << "\n";
  return std::string("<unexpected builtin>");
}

std::string
getScalarValueAsString(clang::ASTContext const &AST,
                       clang::Type const *Type,
                       stateptr_ty const Address,
                       seec::trace::MemoryState const &Memory)
{
  auto const CanonQualType = Type->getCanonicalTypeInternal();
  auto const CanonType = CanonQualType.getTypePtr();
  
  switch (CanonQualType->getTypeClass()) {
    // BuiltinType
    case ::clang::Type::Builtin:
    {
      auto const Ty = llvm::cast< ::clang::BuiltinType>(CanonType);
      return getScalarValueAsString(AST, Ty, Address, Memory);
    }
    
    // AtomicType
    case ::clang::Type::Atomic:
    {
      // Recursive on the underlying type.
      auto const Ty = llvm::cast< ::clang::AtomicType>(CanonType);
      auto const ValueTy = Ty->getValueType().getCanonicalType().getTypePtr();
      return getScalarValueAsString(AST, ValueTy, Address, Memory);
    }
    
    // EnumType
    case ::clang::Type::Enum:
    {
      // Recursive on the underlying type.
      auto const Ty = llvm::cast< ::clang::EnumType>(CanonType);
      auto const IntegerTy = Ty->getDecl()->getIntegerType();
      auto const CanonicalIntegerTy = IntegerTy.getCanonicalType().getTypePtr();
      return getScalarValueAsString(AST, CanonicalIntegerTy, Address, Memory);
    }
    
    // PointerType
    case ::clang::Type::Pointer:
    {
      auto const MaybeValue = readAPIntFromMemory(AST, Type, Address, Memory);
      if (MaybeValue.assigned<llvm::APInt>()) {
        return std::string{"0x"}
               + MaybeValue.get<llvm::APInt>().toString(16, false);
      }
      return std::string{"<unassigned value>"};
    }
    
#define SEEC_UNHANDLED_TYPE_CLASS(CLASS)                                       \
    case ::clang::Type::CLASS:                                                 \
      return std::string("<type class " #CLASS " not implemented>");
    
    // Not needed because this function is only for scalars.
    SEEC_UNHANDLED_TYPE_CLASS(Complex)
    SEEC_UNHANDLED_TYPE_CLASS(Record)
    SEEC_UNHANDLED_TYPE_CLASS(ConstantArray)
    SEEC_UNHANDLED_TYPE_CLASS(IncompleteArray)
    SEEC_UNHANDLED_TYPE_CLASS(VariableArray)
    
    // Not needed because we don't support the language(s).
    SEEC_UNHANDLED_TYPE_CLASS(BlockPointer) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(LValueReference) // C++
    SEEC_UNHANDLED_TYPE_CLASS(RValueReference) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(MemberPointer) // C++
    SEEC_UNHANDLED_TYPE_CLASS(Auto) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(DeducedTemplateSpecialization) // C++17
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObject) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCInterface) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObjectPointer) //ObjC
    SEEC_UNHANDLED_TYPE_CLASS(Pipe)
    
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
  /// \c ASTContext for the \c Type of this value.
  clang::ASTContext const &AST;

  /// The canonical Type of this value.
  ::clang::Type const * CanonicalType;
  
  /// The recorded memory address of the value.
  stateptr_ty Address;
  
  /// The size of the value.
  ::clang::CharUnits Size;
  
  /// The state of recorded memory.
  seec::trace::MemoryState const &Memory;
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    return Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
  }

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
    auto const Values = Memory.getRegion(MemoryArea(Address,
                                                    Size.getQuantity()))
                              .getByteValues();

    return std::all_of(Values.begin(), Values.end(),
                       [] (char const V) { return V == 0; });
  }
  
public:
  /// \brief Constructor.
  ///
  ValueByMemoryForScalar(::clang::Type const *WithCanonicalType,
                         stateptr_ty WithAddress,
                         ::clang::CharUnits WithSize,
                         seec::trace::ProcessState const &ForProcessState,
                         clang::ASTContext const &WithAST)
  : ValueOfScalar(),
    AST(WithAST),
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
  virtual stateptr_ty getAddress() const override { return Address; }
  
  virtual bool isCompletelyInitialized() const override {
    auto Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
    return Region.isCompletelyInitialized();
  }
  
  virtual bool isPartiallyInitialized() const override {
    auto Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
    return Region.isPartiallyInitialized();
  }
  
  virtual std::string getValueAsStringShort() const override {
    if (!isCompletelyInitialized())
      return std::string("<uninitialized>");

    return getScalarValueAsString(AST, CanonicalType, Address, Memory);
  }
  
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
};


//===----------------------------------------------------------------------===//
// ValueByMemoryForComplex
//===----------------------------------------------------------------------===//

/// \brief Represents a complex Value in memory.
///
class ValueByMemoryForComplex final : public ValueOfComplex {
  /// \c ASTContext for the \c Type of this value.
  clang::ASTContext const &AST;

  /// The canonical Type of this value.
  ::clang::ComplexType const * CanonicalType;

  /// The recorded memory address of the value.
  stateptr_ty Address;

  /// The size of the value.
  ::clang::CharUnits Size;

  /// The state of recorded memory.
  seec::trace::MemoryState const &Memory;

  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    return Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
  }

  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return Size;
  }

public:
  /// \brief Constructor.
  ///
  ValueByMemoryForComplex(::clang::ComplexType const *WithCanonicalType,
                          stateptr_ty WithAddress,
                          ::clang::CharUnits WithSize,
                          seec::trace::ProcessState const &ForProcessState,
                          clang::ASTContext const &WithAST)
  : ValueOfComplex(),
    AST(WithAST),
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
  virtual stateptr_ty getAddress() const override { return Address; }

  virtual bool isCompletelyInitialized() const override {
    auto Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
    return Region.isCompletelyInitialized();
  }

  virtual bool isPartiallyInitialized() const override {
    auto Region = Memory.getRegion(MemoryArea(Address, Size.getQuantity()));
    return Region.isPartiallyInitialized();
  }

  virtual std::string getValueAsStringShort() const override {
    if (!isCompletelyInitialized())
      return std::string("<uninitialized>");

    return getScalarValueAsString(AST, CanonicalType, Address, Memory);
  }

  virtual std::string getValueAsStringFull() const override {
    std::string Ret;

    auto const ElemTy =
      llvm::dyn_cast<clang::BuiltinType>
                    (CanonicalType->getElementType().getTypePtr());

    auto const ElemSize = AST.getTypeSizeInChars(ElemTy);

    auto const RealAddr = Address;
    auto const ImagAddr = RealAddr + ElemSize.getQuantity();

    Ret = getScalarValueAsString(AST, ElemTy, RealAddr, Memory);
    auto const ImagStr = getScalarValueAsString(AST, ElemTy, ImagAddr, Memory);

    if (ImagStr.size() == 0 || ImagStr[0] != '-')
      Ret.push_back('+');
    Ret += ImagStr;
    Ret.push_back('i');

    return Ret;
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
  stateptr_ty Address;
  
  /// The size of the pointee type.
  ::clang::CharUnits PointeeSize;
  
  /// The raw value of this pointer.
  stateptr_ty RawValue;
  
  /// The ProcessState that this value is for.
  seec::trace::ProcessState const &ProcessState;
  
  /// \brief Constructor.
  ///
  ValueByMemoryForPointer(std::weak_ptr<ValueStore const> InStore,
                          ::clang::ASTContext const &WithASTContext,
                          ::clang::Type const *WithCanonicalType,
                          stateptr_ty WithAddress,
                          ::clang::CharUnits WithPointeeSize,
                          stateptr_ty WithRawValue,
                          seec::trace::ProcessState const &ForProcessState)
  : Store(InStore),
    ASTContext(WithASTContext),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    PointeeSize(WithPointeeSize),
    RawValue(WithRawValue),
    ProcessState(ForProcessState)
  {}
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    MemoryArea const Area {Address,
                           std::size_t(getTypeSizeInCharsImpl().getQuantity())};
    return ProcessState.getMemory().getRegion(Area);
  }

  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ASTContext.getTypeSizeInChars(CanonicalType);
  }
  
  /// \brief Check if this is a valid opaque pointer (e.g. a DIR *).
  ///
  virtual bool isValidOpaqueImpl() const override {
    return ProcessState.getDir(RawValue) != nullptr
        || ProcessState.getStream(RawValue) != nullptr;
  }
  
  /// \brief Get the raw value of this pointer.
  ///
  virtual stateptr_ty getRawValueImpl() const override {
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
         stateptr_ty Address,
         seec::trace::ProcessState const &ProcessState)
  {
    // Get the size of pointee type.
    auto const Type = CanonicalType->getAs< ::clang::PointerType>();
    assert(Type && "Expected PointerType");
    
    auto const PointeeQType = Type->getPointeeType().getCanonicalType();
    auto const PointeeSize = PointeeQType->isIncompleteType()
                           ? ::clang::CharUnits::fromQuantity(0)
                           : ASTContext.getTypeSizeInChars(PointeeQType);
    
    // Get the raw pointer value.
    auto const MaybeValue = readAPIntFromMemory(ASTContext,
                                                Type,
                                                Address,
                                                ProcessState.getMemory());

    auto const PtrValue = MaybeValue.assigned<llvm::APInt>()
                        ? MaybeValue.get<llvm::APInt>().getLimitedValue()
                        : 0;

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
  virtual stateptr_ty getAddress() const override { return Address; }
  
  virtual bool isCompletelyInitialized() const override {
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(void const *)));
    return Region.isCompletelyInitialized();
  }
  
  virtual bool isPartiallyInitialized() const override {
    auto const &Memory = ProcessState.getMemory();
    auto Region = Memory.getRegion(MemoryArea(Address, sizeof(void const *)));
    return Region.isPartiallyInitialized();
  }
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  virtual std::string getValueAsStringShort() const override {
    if (!isCompletelyInitialized())
      return std::string("<uninitialized>");

    std::string RetString;

    {
      llvm::raw_string_ostream Stream(RetString);
      Stream << "0x";
      Stream.write_hex(RawValue);
    } // destruction of Stream will flush to RetString.

    return RetString;
  }
  
  /// Get a string describing the value.
  ///
  virtual std::string getValueAsStringFull() const override {
    return getValueAsStringShort();
  }
  
  /// \brief Get the highest legal dereference of this value.
  ///
  virtual int getDereferenceIndexLimit() const override {
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
  getDereferenced(int Index) const override {
    // Find the Store (if it still exists).
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<Value const>();
    
    auto const Address = RawValue + (Index * PointeeSize.getQuantity());
    
    return getValue(StorePtr,
                    CanonicalType->getPointeeType(),
                    ASTContext,
                    Address,
                    ProcessState,
                    /* OwningFunction */ nullptr);
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
  stateptr_ty Address;
  
  /// The process state that this Value is in.
  seec::trace::ProcessState const &ProcessState;
  
  /// \brief Constructor.
  ///
  ValueByMemoryForRecord(std::weak_ptr<ValueStore const> InStore,
                         ::clang::ASTContext const &WithASTContext,
                         ::clang::ASTRecordLayout const &WithLayout,
                         ::clang::Type const *WithCanonicalType,
                         stateptr_ty WithAddress,
                         seec::trace::ProcessState const &ForProcessState)
  : Store(InStore),
    ASTContext(WithASTContext),
    Layout(WithLayout),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    ProcessState(ForProcessState)
  {}
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    MemoryArea const Area {Address,
                           std::size_t(getTypeSizeInCharsImpl().getQuantity())};
    return ProcessState.getMemory().getRegion(Area);
  }

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
         stateptr_ty Address,
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
  virtual stateptr_ty getAddress() const override { return Address; }
  
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
                    ProcessState,
                    /* OwningFunction */ nullptr);
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
  stateptr_ty Address;
  
  /// The size of an element.
  unsigned ElementSize;
  
  /// The number of elements.
  unsigned ElementCount;
  
  /// The process state that this Value is in.
  seec::trace::ProcessState const &ProcessState;

  /// The function state that this Value is in.
  seec::trace::FunctionState const *OwningFunction;

  /// \brief Constructor.
  ///
  ValueByMemoryForArray(std::weak_ptr<ValueStore const> InStore,
                        ::clang::ASTContext const &WithASTContext,
                        ::clang::ArrayType const *WithCanonicalType,
                        stateptr_ty WithAddress,
                        unsigned WithElementSize,
                        unsigned WithElementCount,
                        seec::trace::ProcessState const &ForProcessState,
                        seec::trace::FunctionState const *WithOwningFunction)
  : Store(InStore),
    ASTContext(WithASTContext),
    CanonicalType(WithCanonicalType),
    Address(WithAddress),
    ElementSize(WithElementSize),
    ElementCount(WithElementCount),
    ProcessState(ForProcessState),
    OwningFunction(WithOwningFunction)
  {}
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    MemoryArea const Area {Address, ElementSize * ElementCount};
    return ProcessState.getMemory().getRegion(Area);
  }

  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return ASTContext.getTypeSizeInChars(CanonicalType);
  }

  /// \brief Get the size of each child in this value.
  ///
  virtual std::size_t getChildSizeImpl() const override {
    return ElementSize;
  }
  
  /// \brief Get the runtime value of a \c VariableArrayType size expression.
  ///
  static Maybe<uint64_t>
  getValueOfSizeExpr(clang::VariableArrayType const *Type,
                     trace::FunctionState const &State,
                     seec_clang::MappedModule const &Mapping)
  {
    auto const MappedStmt =
      Mapping.getMappedStmtForStmt(Type->getSizeExpr());

    if (!MappedStmt ||
        MappedStmt->getMapType() != seec_clang::MappedStmt::Type::RValScalar)
    {
      llvm::errs() << "VariableArrayType size expr unmapped.\n";
      return Maybe<uint64_t>();
    }

    auto const MaybeSize =
      seec::trace::getAPInt(State, MappedStmt->getValue());

    if (!MaybeSize) {
      llvm::errs() << "VariableArrayType size expr unresolvable.\n";
      llvm::errs() << *(MappedStmt->getValue()) << "\n";
      return Maybe<uint64_t>();
    }

    return MaybeSize->getZExtValue();
  }

  static Maybe<uint64_t>
  calculateElementTypeSize(clang::ASTContext const &ASTContext,
                           clang::Type const *Type,
                           trace::FunctionState const &OwningFunction,
                           seec_clang::MappedModule const &Mapping)
  {
    auto const Size = ASTContext.getTypeSizeInChars(Type);
    if (!Size.isZero())
      return uint64_t(Size.getQuantity());

    if (auto const VAType = llvm::dyn_cast<clang::VariableArrayType>(Type)) {
      auto const ElemSize =
        calculateElementTypeSize(ASTContext,
                                 VAType->getElementType().getTypePtr(),
                                 OwningFunction,
                                 Mapping);
      if (!ElemSize.assigned<uint64_t>())
        return Maybe<uint64_t>();

      auto const Count = getValueOfSizeExpr(VAType, OwningFunction, Mapping);
      if (!Count.assigned<uint64_t>())
        return Maybe<uint64_t>();

      return ElemSize.get<uint64_t>() * Count.get<uint64_t>();
    }

    return uint64_t(0);
  }

public:
  /// \brief Attempt to create a new instance of this class.
  ///
  static std::shared_ptr<ValueByMemoryForArray const>
  create(std::weak_ptr<ValueStore const> Store,
         ::clang::ASTContext const &ASTContext,
         ::clang::Type const *CanonicalType,
         stateptr_ty Address,
         seec::trace::ProcessState const &ProcessState,
         seec::trace::FunctionState const *OwningFunction)
  {
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<ValueByMemoryForArray const>();

    auto const &Mapping = StorePtr->getImpl().getMapping();
    auto const ArrayTy = llvm::cast< ::clang::ArrayType>(CanonicalType);
    auto const ElementTy = ArrayTy->getElementType();

    unsigned ElementSize = ASTContext.getTypeSizeInChars(ElementTy)
                                     .getQuantity();
    
    if (!ElementSize) {
      if (OwningFunction) {
        auto const MaybeSize =
          calculateElementTypeSize(ASTContext,
                                   ElementTy.getTypePtr(),
                                   *OwningFunction,
                                   Mapping);

        if (MaybeSize.assigned<uint64_t>())
          ElementSize = MaybeSize.get<uint64_t>();
      }

      if (!ElementSize)
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
      
      // Attempt to get the runtime value generated by the size
      // expression, but we would need access to the function state.
      case ::clang::Type::TypeClass::VariableArray:
        if (OwningFunction) {
          auto const Ty = llvm::cast< ::clang::VariableArrayType>(ArrayTy);
          auto const MaybeCount = getValueOfSizeExpr(Ty, *OwningFunction,
                                                     Mapping);

          if (MaybeCount.assigned<uint64_t>())
            ElementCount = MaybeCount.get<uint64_t>();
          else
            llvm::errs() << "couldn't resolve VariableArrayType size\n";
        }
        else {
          llvm::errs() << "VariableArray with no owning function\n";
        }
        break;

      // Use whatever size fills the allocated memory block.
      case ::clang::Type::TypeClass::IncompleteArray:
      {
        auto const MaybeArea = ProcessState.getContainingMemoryArea(Address);
        if (MaybeArea.assigned<MemoryArea>()) {
          auto const Area = MaybeArea.get<MemoryArea>().withStart(Address);
          ElementCount = Area.length() / ElementSize;
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
                                                     ElementSize,
                                                     ElementCount,
                                                     ProcessState,
                                                     OwningFunction));
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
  virtual stateptr_ty getAddress() const override { return Address; }
  
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
                    ProcessState,
                    OwningFunction);
  }
};


//===----------------------------------------------------------------------===//
// getScalarValueAsAPSInt() - from llvm::Value
//===----------------------------------------------------------------------===//

llvm::Optional<llvm::APSInt>
getScalarValueAsAPSInt(seec::trace::FunctionState const &State,
                       ::clang::BuiltinType const *Type,
                       ::llvm::Value const *Value)
{
  switch (Type->getKind()) {
#define SEEC_HANDLE_BUILTIN_UNSIGNED(KIND, HOST_TYPE)                          \
    case clang::BuiltinType::KIND:                                             \
      return seec::trace::getAPSIntUnsigned(State, Value);

#define SEEC_HANDLE_BUILTIN_SIGNED(KIND, HOST_TYPE)                            \
    case clang::BuiltinType::KIND:                                             \
      return seec::trace::getAPSIntSigned(State, Value);

#define SEEC_UNHANDLED_BUILTIN(KIND)                                           \
    case clang::BuiltinType::KIND:                                             \
      return llvm::Optional<llvm::APSInt>();

#define SEEC_UNHANDLED_OCLIMAGE(KIND)                                          \
case clang::BuiltinType::KIND##RO: SEEC_FALLTHROUGH;                           \
case clang::BuiltinType::KIND##WO: SEEC_FALLTHROUGH;                           \
case clang::BuiltinType::KIND##RW:                                             \
  return llvm::Optional<llvm::APSInt>();

    // Builtin types
    SEEC_UNHANDLED_BUILTIN(Void)
    
    // Unsigned types
    SEEC_HANDLE_BUILTIN_UNSIGNED(Bool, bool)
    SEEC_HANDLE_BUILTIN_UNSIGNED(Char_U, char)
    SEEC_HANDLE_BUILTIN_UNSIGNED(UChar, unsigned char)
    SEEC_HANDLE_BUILTIN_UNSIGNED(WChar_U, wchar_t)
    SEEC_HANDLE_BUILTIN_UNSIGNED(Char16, char16_t)
    SEEC_HANDLE_BUILTIN_UNSIGNED(Char32, char32_t)
    SEEC_HANDLE_BUILTIN_UNSIGNED(UShort, unsigned short)
    SEEC_HANDLE_BUILTIN_UNSIGNED(UInt, unsigned int)
    SEEC_HANDLE_BUILTIN_UNSIGNED(ULong, unsigned long)
    SEEC_HANDLE_BUILTIN_UNSIGNED(ULongLong, unsigned long long)
    SEEC_UNHANDLED_BUILTIN(UInt128)

    // Signed types
    SEEC_HANDLE_BUILTIN_SIGNED(Char_S, char)
    SEEC_HANDLE_BUILTIN_SIGNED(SChar, signed char)
    SEEC_HANDLE_BUILTIN_SIGNED(WChar_S, wchar_t)
    SEEC_HANDLE_BUILTIN_SIGNED(Short, short)
    SEEC_HANDLE_BUILTIN_SIGNED(Int, int)
    SEEC_HANDLE_BUILTIN_SIGNED(Long, long)
    SEEC_HANDLE_BUILTIN_SIGNED(LongLong, long long)
    SEEC_UNHANDLED_BUILTIN(Int128)

    // Floating point types
    SEEC_UNHANDLED_BUILTIN(Half)
    SEEC_UNHANDLED_BUILTIN(Float)
    SEEC_UNHANDLED_BUILTIN(Double)
    SEEC_UNHANDLED_BUILTIN(LongDouble)
    SEEC_UNHANDLED_BUILTIN(Float16)
    SEEC_UNHANDLED_BUILTIN(Float128)
    
    // Language-specific types
    SEEC_UNHANDLED_BUILTIN(NullPtr)
    SEEC_UNHANDLED_BUILTIN(ObjCId)
    SEEC_UNHANDLED_BUILTIN(ObjCClass)
    SEEC_UNHANDLED_BUILTIN(ObjCSel)

    SEEC_UNHANDLED_OCLIMAGE(OCLImage1d)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage1dArray)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage1dBuffer)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2d)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArray)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dDepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArrayDepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dMSAA)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArrayMSAA)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dMSAADepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArrayMSAADepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage3d)

    SEEC_UNHANDLED_BUILTIN(OCLSampler)
    SEEC_UNHANDLED_BUILTIN(OCLEvent)
    SEEC_UNHANDLED_BUILTIN(OCLClkEvent)
    SEEC_UNHANDLED_BUILTIN(OCLQueue)
    SEEC_UNHANDLED_BUILTIN(OCLReserveID)
    SEEC_UNHANDLED_BUILTIN(Dependent)
    SEEC_UNHANDLED_BUILTIN(Overload)
    SEEC_UNHANDLED_BUILTIN(BoundMember)
    SEEC_UNHANDLED_BUILTIN(PseudoObject)
    SEEC_UNHANDLED_BUILTIN(UnknownAny)
    SEEC_UNHANDLED_BUILTIN(BuiltinFn)
    SEEC_UNHANDLED_BUILTIN(ARCUnbridgedCast)
    SEEC_UNHANDLED_BUILTIN(OMPArraySection)

#undef SEEC_HANDLE_BUILTIN_UNSIGNED
#undef SEEC_HANDLE_BUILTIN_SIGNED
#undef SEEC_UNHANDLED_BUILTIN
#undef SEEC_UNHANDLED_OCLIMAGE
  }
  
  llvm_unreachable("unexpected builtin type");
  return llvm::Optional<llvm::APSInt>();
}

llvm::Optional<llvm::APSInt>
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
      return llvm::Optional<llvm::APSInt>();
  }
}


//===----------------------------------------------------------------------===//
// getScalarValueAsString() - from llvm::Value
//===----------------------------------------------------------------------===//

template<typename T, typename Enable = void>
struct GetValueOfBuiltinAsString; // undefined

template<typename T>
struct GetValueOfBuiltinAsString
<T, typename enable_if<is_integral<T>::value && is_signed<T>::value>::type>
{
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getAPSIntSigned(State, Value);
    if (MaybeValue)
      return MaybeValue->toString(10);
    else
      return std::string("<") + __PRETTY_FUNCTION__ + ": failed>";
  }
};

template<typename T>
struct GetValueOfBuiltinAsString
<T, typename enable_if<is_integral<T>::value && is_unsigned<T>::value>::type>
{
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getAPInt(State, Value);
    if (MaybeValue)
      return MaybeValue->toString(10, false);
    else
      return std::string("<") + __PRETTY_FUNCTION__ + ": failed>";
  }
};

template<typename T>
struct GetValueOfBuiltinAsString
<T, typename enable_if<is_floating_point<T>::value>::type>
{
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getAPFloat(State, Value);
    if (MaybeValue) {
      llvm::SmallString<32> Buffer;
      MaybeValue->toString(Buffer);
      return Buffer.str().str();
    }
    else
      return std::string("<") + __PRETTY_FUNCTION__ + ": failed>";
  }
};

template<>
struct GetValueOfBuiltinAsString<void const *> {
  static std::string impl(seec::trace::FunctionState const &State,
                          ::llvm::Value const *Value)
  {
    auto const MaybeValue = seec::trace::getAPInt(State, Value);
    if (!MaybeValue)
      return std::string("<void const *: couldn't get current runtime value>");

    return std::string{"0x"} + MaybeValue->toString(16, false);
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

#define SEEC_UNHANDLED_OCLIMAGE(KIND)                                          \
case clang::BuiltinType::KIND##RO: SEEC_FALLTHROUGH;                           \
case clang::BuiltinType::KIND##WO: SEEC_FALLTHROUGH;                           \
case clang::BuiltinType::KIND##RW:                                             \
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
    SEEC_UNHANDLED_BUILTIN(Float16)
    SEEC_UNHANDLED_BUILTIN(Float128)

    // Language-specific types
    SEEC_UNHANDLED_BUILTIN(NullPtr)
    SEEC_UNHANDLED_BUILTIN(ObjCId)
    SEEC_UNHANDLED_BUILTIN(ObjCClass)
    SEEC_UNHANDLED_BUILTIN(ObjCSel)
    
    SEEC_UNHANDLED_OCLIMAGE(OCLImage1d)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage1dArray)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage1dBuffer)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2d)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArray)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dDepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArrayDepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dMSAA)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArrayMSAA)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dMSAADepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage2dArrayMSAADepth)
    SEEC_UNHANDLED_OCLIMAGE(OCLImage3d)
    
    SEEC_UNHANDLED_BUILTIN(OCLSampler)
    SEEC_UNHANDLED_BUILTIN(OCLEvent)
    SEEC_UNHANDLED_BUILTIN(OCLClkEvent)
    SEEC_UNHANDLED_BUILTIN(OCLQueue)
    SEEC_UNHANDLED_BUILTIN(OCLReserveID)
    SEEC_UNHANDLED_BUILTIN(Dependent)
    SEEC_UNHANDLED_BUILTIN(Overload)
    SEEC_UNHANDLED_BUILTIN(BoundMember)
    SEEC_UNHANDLED_BUILTIN(PseudoObject)
    SEEC_UNHANDLED_BUILTIN(UnknownAny)
    SEEC_UNHANDLED_BUILTIN(BuiltinFn)
    SEEC_UNHANDLED_BUILTIN(ARCUnbridgedCast)
    SEEC_UNHANDLED_BUILTIN(OMPArraySection)

#undef SEEC_HANDLE_BUILTIN
#undef SEEC_UNHANDLED_BUILTIN
#undef SEEC_UNHANDLED_OCLIMAGE
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
        
        if (MaybeIntVal) {
          auto const &IntVal = *MaybeIntVal;
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
      auto const MaybeInt = seec::trace::getAPInt(State, Value);
      if (!MaybeInt)
        return std::string("<pointer: couldn't get value>");
      
      return std::string("0x") + MaybeInt->toString(16, false);
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
    SEEC_UNHANDLED_TYPE_CLASS(DeducedTemplateSpecialization) // C++17
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObject) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCInterface) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObjectPointer) //ObjC
    SEEC_UNHANDLED_TYPE_CLASS(Pipe) // OpenCL
    
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
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    return seec::Maybe<seec::trace::MemoryStateRegion>();
  }

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
    
    if (!Val)
      return false;
    
    return *Val == 0;
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
  virtual stateptr_ty getAddress() const override { return 0; }
  
  /// \brief Runtime values are always initialized.
  ///
  virtual bool isCompletelyInitialized() const override { return true; }
  
  /// \brief Runtime values are always initialized.
  ///
  virtual bool isPartiallyInitialized() const override { return true; }
  
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
// ValueByRuntimeValueForComplex
//===----------------------------------------------------------------------===//

/// \brief Represents a complex Value in LLVM's virtual registers.
///
class ValueByRuntimeValueForComplex final : public ValueOfComplex {
  /// The Expr that this value is for.
  ::clang::Expr const *m_Expression;

  /// The FunctionState that this value is for.
  seec::trace::FunctionState const &m_FunctionState;

  /// The LLVM Value for the real part of this value.
  llvm::Value const *m_Real;

  /// The LLVM Value for the imaginary part of this value.
  llvm::Value const *m_Imag;

  /// The size of this value.
  ::clang::CharUnits m_TypeSize;

  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    return seec::Maybe<seec::trace::MemoryStateRegion>();
  }

  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const override {
    return m_TypeSize;
  }

public:
  /// \brief Constructor.
  ///
  ValueByRuntimeValueForComplex(::clang::Expr const *ForExpression,
                               seec::trace::FunctionState const &ForState,
                               llvm::Value const *WithRealValue,
                               llvm::Value const *WithImagValue,
                               ::clang::CharUnits WithTypeSizeInChars)
  : ValueOfComplex(),
    m_Expression(ForExpression),
    m_FunctionState(ForState),
    m_Real(WithRealValue),
    m_Imag(WithImagValue),
    m_TypeSize(WithTypeSizeInChars)
  {}

  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const override {
    return m_Expression->getType().getCanonicalType().getTypePtr();
  }

  /// \brief Get the Expr that this Value is for.
  ///
  virtual ::clang::Expr const *getExpr() const override { return m_Expression; }

  /// \brief Runtime values are never in memory.
  ///
  virtual bool isInMemory() const override { return false; }

  /// \brief Get the address in memory.
  ///
  /// pre: isInMemory() == true
  ///
  virtual stateptr_ty getAddress() const override { return 0; }

  /// \brief Runtime values are always initialized.
  ///
  virtual bool isCompletelyInitialized() const override { return true; }

  /// \brief Runtime values are always initialized.
  ///
  virtual bool isPartiallyInitialized() const override { return true; }

  /// \brief Get a string describing the value (which may be elided).
  ///
  virtual std::string getValueAsStringShort() const override {
    auto const MaybeReal = getAPFloat(m_FunctionState, m_Real);
    if (!MaybeReal)
      return "<real part not found>";

    auto const MaybeImag = getAPFloat(m_FunctionState, m_Imag);
    if (!MaybeImag)
      return "<imaginary part not found>";

    auto const &Real = *MaybeReal;
    auto const &Imag = *MaybeImag;

    llvm::SmallString<32> TheString;
    Real.toString(TheString);

    if (!Imag.isInfinity() && !Imag.isNegative())
      TheString.push_back('+');
    Imag.toString(TheString);
    TheString.push_back('i');
    return TheString.str().str();
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
  stateptr_ty PtrValue;
  
  /// The size of the pointee type.
  ::clang::CharUnits PointeeSize;
  
  /// \brief Constructor.
  ///
  ValueByRuntimeValueForPointer(std::weak_ptr<ValueStore const> WithStore,
                                ::clang::Expr const *ForExpression,
                                seec::seec_clang::MappedAST const &WithAST,
                                seec::trace::ProcessState const &ForState,
                                stateptr_ty WithPtrValue,
                                ::clang::CharUnits WithPointeeSize)
  : Store(WithStore),
    Expression(ForExpression),
    MappedAST(WithAST),
    ProcessState(ForState),
    PtrValue(WithPtrValue),
    PointeeSize(WithPointeeSize)
  {}
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const override {
    return seec::Maybe<seec::trace::MemoryStateRegion>();
  }

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
    return ProcessState.getDir(PtrValue) != nullptr
        || ProcessState.getStream(PtrValue) != nullptr;
  }
  
  /// \brief Get the raw value of this pointer.
  ///
  virtual stateptr_ty getRawValueImpl() const override { return PtrValue; }
  
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
    auto const MaybeValue = seec::trace::getAPInt(FunctionState, LLVMValue);
    if (!MaybeValue)
      return std::shared_ptr<ValueByRuntimeValueForPointer>();
    
    auto const PtrValue = MaybeValue->getLimitedValue();
    
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
  virtual stateptr_ty getAddress() const override { return 0; }
  
  /// \brief Runtime values are always initialized.
  ///
  virtual bool isCompletelyInitialized() const override { return true; }
  
  /// \brief Runtime values are always partially initialized.
  ///
  virtual bool isPartiallyInitialized() const override { return true; }
  
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
  virtual int getDereferenceIndexLimit() const override {
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
  getDereferenced(int Index) const override {
    // Get the store (if it still exists).
    auto StorePtr = Store.lock();
    if (!StorePtr)
      return std::shared_ptr<Value const>();
    
    auto const Address = PtrValue + (Index * PointeeSize.getQuantity());
    
    return getValue(StorePtr,
                    Expression->getType()->getPointeeType(),
                    MappedAST.getASTUnit().getASTContext(),
                    Address,
                    ProcessState,
                    /* OwningFunction */ nullptr);
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
            stateptr_ty Address,
            seec::trace::ProcessState const &ProcessState,
            seec::trace::FunctionState const *OwningFunction)
{
  if (!QualType.getTypePtr()) {
    llvm_unreachable("null type");
    return std::shared_ptr<Value const>();
  }
  
  auto const CanonicalType = QualType.getCanonicalType();
  if (CanonicalType->isIncompleteType()) {
    llvm::errs() << "can't create value for incomplete type: "
                 << CanonicalType.getAsString() << "\n";
    return std::shared_ptr<Value const>(); // No values for incomplete types.
  }
  
  auto TypeSize = ASTContext.getTypeSizeInChars(CanonicalType);
  
  switch (CanonicalType->getTypeClass()) {
    // Scalar values.
    case ::clang::Type::Builtin:
    {
      // Correct the size of long double to only consider the used bytes.
      auto const BT = llvm::dyn_cast< ::clang::BuiltinType >(CanonicalType);
      if (BT->getKind() == clang::BuiltinType::LongDouble) {
        auto const &Semantics = ASTContext.getFloatTypeSemantics(CanonicalType);
        if (&Semantics == &llvm::APFloat::x87DoubleExtended()) {
          TypeSize = ::clang::CharUnits::fromQuantity(10);
        }
      }

      SEEC_FALLTHROUGH;
    }
    case ::clang::Type::Atomic:  SEEC_FALLTHROUGH;
    case ::clang::Type::Enum:
    {
      return std::make_shared<ValueByMemoryForScalar>
                             (CanonicalType.getTypePtr(),
                              Address,
                              TypeSize,
                              ProcessState,
                              ASTContext);
    }

    case ::clang::Type::Complex:
    {
      auto const CT = llvm::dyn_cast< ::clang::ComplexType>(CanonicalType);
      return std::make_shared<ValueByMemoryForComplex>
                             (CT,
                              Address,
                              TypeSize,
                              ProcessState,
                              ASTContext);
    }
    
    case ::clang::Type::Pointer:
    {
      return ValueByMemoryForPointer::create(Store,
                                             ASTContext,
                                             CanonicalType.getTypePtr(),
                                             Address,
                                             ProcessState);
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
                                           ProcessState,
                                           OwningFunction);
    }
    
#define SEEC_UNHANDLED_TYPE_CLASS(CLASS)                                       \
    case ::clang::Type::CLASS:                                                 \
      return std::shared_ptr<Value const>();
    
    // Not needed because we don't support the language(s).
    SEEC_UNHANDLED_TYPE_CLASS(BlockPointer) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(LValueReference) // C++
    SEEC_UNHANDLED_TYPE_CLASS(RValueReference) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(MemberPointer) // C++
    SEEC_UNHANDLED_TYPE_CLASS(Auto) // C++11
    SEEC_UNHANDLED_TYPE_CLASS(DeducedTemplateSpecialization) // C++17
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObject) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCInterface) // ObjC
    SEEC_UNHANDLED_TYPE_CLASS(ObjCObjectPointer) //ObjC
    SEEC_UNHANDLED_TYPE_CLASS(Pipe) // OpenCL
    
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
// ValueStoreImpl method definitions
//===----------------------------------------------------------------------===//

std::shared_ptr<Value const>
ValueStoreImpl::getValue(std::shared_ptr<ValueStore const> StorePtr,
                         ::clang::QualType QualType,
                         ::clang::ASTContext const &ASTContext,
                         stateptr_ty Address,
                         seec::trace::ProcessState const &ProcessState,
                         seec::trace::FunctionState const *OwningFunction) const
{
  auto const CanonicalType = QualType.getCanonicalType().getTypePtr();
  if (!CanonicalType) {
    llvm::errs() << "can't get value: QualType has no CanonicalType.\n"
                  << "QualType: " << QualType.getAsString() << "\n";
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
                                ProcessState,
                                OwningFunction);
  if (!SharedPtr)
    return SharedPtr;

  // Store a shared_ptr for this Value in the lookup table.
  TypeMap.add(Matcher, SharedPtr);

  return SharedPtr;
}


//===----------------------------------------------------------------------===//
// ValueStore
//===----------------------------------------------------------------------===//

ValueStore::ValueStore(seec::seec_clang::MappedModule const &WithMapping)
: Impl(new ValueStoreImpl(WithMapping))
{}

ValueStore::~ValueStore() = default;

ValueStoreImpl const &ValueStore::getImpl() const {
  return *Impl;
}

std::shared_ptr<Value const>
ValueStore::findFromAddressAndType(stateptr_ty Address,
                                   llvm::StringRef TypeString) const
{
  return Impl->findFromAddressAndType(Address, TypeString);
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
         stateptr_ty Address,
         seec::trace::ProcessState const &ProcessState,
         seec::trace::FunctionState const *OwningFunction)
{
  return Store->getImpl().getValue(Store,
                                   QualType,
                                   ASTContext,
                                   Address,
                                   ProcessState,
                                   OwningFunction);
}


//===----------------------------------------------------------------------===//
// getValue() from a mapped ::clang::Stmt.
//===----------------------------------------------------------------------===//

// Documented in MappedValue.hpp
//
std::shared_ptr<Value const>
getValueImpl(std::shared_ptr<ValueStore const> Store,
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
      auto const MaybeValue = seec::trace::getAPInt(FunctionState,
                                                    SMap.getValue());

      if (!MaybeValue) {
        return std::shared_ptr<Value const>();
      }
      
      auto const PtrValue = MaybeValue->getLimitedValue();
      
      // Get the in-memory value at the given address.
      return getValue(Store,
                      Expression->getType(),
                      SMap.getAST().getASTUnit().getASTContext(),
                      PtrValue,
                      FunctionState.getParent().getParent(),
                      &FunctionState);
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
        if (!FunctionState.hasValue(I)) {
          return std::shared_ptr<Value const>();
        }
      }
      
      auto const ExprType = Expression->getType();
      auto const TypeSize = SMap.getAST()
                                .getASTUnit()
                                .getASTContext()
                                .getTypeSizeInChars(ExprType);

      // Simple scalar value.
      if (LLVMValues.second == nullptr) {
        if (ExprType->getAs<clang::PointerType>()) {
          // Pointer types use a special implementation.
          return ValueByRuntimeValueForPointer::create(Store,
                                                       SMap,
                                                       Expression,
                                                       FunctionState,
                                                       LLVMValues.first);
        }
        else if (ExprType->isIncompleteType()) {
          return std::shared_ptr<Value const>();
        }
        else {
          // All other types use a single implementation.
          return std::make_shared<ValueByRuntimeValueForScalar>
                                 (Expression,
                                  FunctionState,
                                  LLVMValues.first,
                                  TypeSize);
        }
      }
      else { // Complex value.
        // If the second Value is an Instruction, then ensure that it has been
        // evaluated and is still valid.
        if (auto const I = llvm::dyn_cast<llvm::Instruction>(LLVMValues.second))
        {
          if (!FunctionState.hasValue(I)) {
            return std::shared_ptr<Value const>();
          }
        }

        return std::make_shared<ValueByRuntimeValueForComplex>
                              (Expression,
                                FunctionState,
                                LLVMValues.first,
                                LLVMValues.second,
                                TypeSize);
      }
    }
    
    case seec::seec_clang::MappedStmt::Type::RValAggregate:
    {
      // Extract the address of the in-memory object that this rval represents.
      auto const MaybeValue = seec::trace::getAPInt(FunctionState,
                                                    SMap.getValue());

      if (!MaybeValue) {
        return std::shared_ptr<Value const>();
      }
      
      auto const PtrValue = MaybeValue->getLimitedValue();
      
      // Get the in-memory value at the given address.
      return getValue(Store,
                      Expression->getType(),
                      SMap.getAST().getASTUnit().getASTContext(),
                      PtrValue,
                      FunctionState.getParent().getParent(),
                      &FunctionState);
    }
  }
  
  llvm_unreachable("Unhandled MappedStmt::Type!");
  return std::shared_ptr<Value const>();
}

std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         seec::seec_clang::MappedStmt const &SMap,
         seec::trace::FunctionState const &FunctionState)
{
  auto &StoreImpl = Store->getImpl();
  return StoreImpl.getValue(Store, SMap, FunctionState);
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
    case Value::Kind::Complex: SEEC_FALLTHROUGH;
    case Value::Kind::Pointer:
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
  
  auto const L0Size = L0->getTypeSizeInChars().getQuantity();
  auto const R0Size = R0->getTypeSizeInChars().getQuantity();
  if (L0Size != R0Size)
    return false;

  if (LHS.getRawValue() <= RHS.getRawValue()) {
    int  const Offset = (RHS.getRawValue() - LHS.getRawValue()) / L0Size;
    auto const Limit  = LHS.getDereferenceIndexLimit();
    return Offset < Limit && LHS.getDereferenced(Offset) == R0;
  }
  else {
    int  const Offset = (LHS.getRawValue() - RHS.getRawValue()) / R0Size;
    auto const Limit  = RHS.getDereferenceIndexLimit();
    return Offset < Limit && RHS.getDereferenced(Offset) == L0;
  }
  
  return false;
}


} // namespace cm (in seec)

} // namespace seec
