//===- include/seec/Clang/MappedValue.hpp ---------------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDVALUE_HPP
#define SEEC_CLANG_MAPPEDVALUE_HPP


#include "seec/Clang/MappedStateCommon.hpp"
#include "seec/Trace/MemoryState.hpp"
#include "seec/Util/Fallthrough.hpp"

#include "clang/AST/CharUnits.h"
#include "clang/AST/Type.h"

#include "llvm/Support/Casting.h"

#include <functional>
#include <memory>
#include <string>


namespace clang {
  class ASTContext;
  class Expr;
  class FieldDecl;
  class Stmt;
} // namespace clang

namespace seec {

namespace seec_clang {
  class MappedModule;
  class MappedStmt;
} // namespace seec_clang (in seec)

namespace trace {
  class FunctionState;
  class ProcessState;
} // namespace trace (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class StreamState;


/// \brief Represents a runtime value.
///
class Value {
public:
  enum class Kind {
    Basic,
    Scalar,
    Complex,
    Array,
    Record,
    Pointer
  };

private:
  /// General Kind of this Value.
  Kind ThisKind;
  
  /// \brief Get the region of memory that this Value occupies.
  ///
  virtual seec::Maybe<seec::trace::MemoryStateRegion>
  getUnmappedMemoryRegionImpl() const =0;

  /// \brief Get the size of the value's type.
  ///
  virtual ::clang::CharUnits getTypeSizeInCharsImpl() const =0;

public:
  /// \brief Constructor.
  ///
  Value(Kind WithKind)
  : ThisKind(WithKind)
  {}
  
  /// \brief Get the Kind of this Value.
  ///
  Kind getKind() const { return ThisKind; }
  
  /// \brief Virtual destructor required.
  ///
  virtual ~Value() =0;
  
  /// \brief Get the canonical type of this Value.
  ///
  virtual ::clang::Type const *getCanonicalType() const =0;
  
  /// \brief Get the type of this Value as a string.
  ///
  std::string getTypeAsString() const {
    ::clang::QualType QT{getCanonicalType(), /* quals */ 0};
    return QT.getAsString();
  }
  
  /// \brief Get the Expr that this Value is for (if any).
  ///
  virtual ::clang::Expr const *getExpr() const =0;
  
  /// \brief Check if this represents a value stored in memory.
  ///
  virtual bool isInMemory() const =0;
  
  /// \brief Get the address in memory.
  /// 
  /// pre: isInMemory() == true
  ///
  virtual stateptr_ty getAddress() const =0;

  /// \brief Get the region of memory that this Value occupies.
  ///
  seec::Maybe<seec::trace::MemoryStateRegion> getUnmappedMemoryRegion() const {
    return getUnmappedMemoryRegionImpl();
  }
  
  /// \brief Get the size of the value's type.
  ///
  ::clang::CharUnits getTypeSizeInChars() const {
    return getTypeSizeInCharsImpl();
  }
  
  /// \brief Check if this value is completely initialized.
  ///
  /// If this is an aggregate value, then the result of this method is the
  /// logical AND reduction of applying this operation to all children.
  ///
  virtual bool isCompletelyInitialized() const =0;
  
  /// \brief Check if this value is partially initialized.
  ///
  /// If this is an aggregate value, then the result of this method is the
  /// logical OR reduction of applying this operation to all children.
  ///
  virtual bool isPartiallyInitialized() const =0;
  
  /// \brief Get a string describing the value (which may be elided).
  ///
  virtual std::string getValueAsStringShort() const =0;
  
  /// \brief Get a string describing the value.
  ///
  virtual std::string getValueAsStringFull() const =0;
};


/// \brief Represents a scalar's runtime value.
///
class ValueOfScalar : public Value {
  /// \brief Check if this value is zero.
  ///
  /// pre: isCompletelyInitialized() == true
  ///
  virtual bool isZeroImpl() const =0;
  
public:
  /// \brief Constructor.
  ///
  ValueOfScalar()
  : Value(Value::Kind::Scalar)
  {}
  
  /// \brief Implement LLVM-style RTTI.
  ///
  static bool classof(Value const *V) {
    return V->getKind() == Value::Kind::Scalar;
  }
  
  /// \brief Check if this value is zero.
  ///
  /// pre: isCompletelyInitialized() == true
  ///
  bool isZero() const { return isZeroImpl(); }
};


/// \brief Represents a complex runtime value.
///
class ValueOfComplex : public Value {
public:
  /// \brief Constructor.
  ///
  ValueOfComplex()
  : Value(Value::Kind::Complex)
  {}

  /// \brief Implement LLVM-style RTTI.
  ///
  static bool classof(Value const *V) {
    return V->getKind() == Value::Kind::Complex;
  }
};


/// \brief Represents an aggregate's runtime value.
///
class ValueOfArray : public Value {
  /// \brief Get the size of each child in this value.
  ///
  virtual std::size_t getChildSizeImpl() const =0;

public:
  /// \brief Constructor.
  ///
  ValueOfArray()
  : Value(Value::Kind::Array)
  {}
  
  /// \brief Implement LLVM-style RTTI.
  ///
  static bool classof(Value const *V) {
    return V->getKind() == Value::Kind::Array;
  }
  
  /// \brief Get the number of children of this value.
  ///
  virtual unsigned getChildCount() const =0;
  
  /// \brief Get a child of this value.
  ///
  virtual std::shared_ptr<Value const> getChildAt(unsigned Index) const =0;

  /// \brief Get the size of each child in this value.
  ///
  std::size_t getChildSize() const { return getChildSizeImpl(); }
};


/// \brief Represents a record's runtime value.
///
class ValueOfRecord : public Value {
public:
  /// \brief Constructor.
  ///
  ValueOfRecord()
  : Value(Value::Kind::Record)
  {}
  
  /// \brief Implement LLVM-style RTTI.
  ///
  static bool classof(Value const *V) {
    return V->getKind() == Value::Kind::Record;
  }
  
  /// \brief Get the number of children of this value.
  ///
  virtual unsigned getChildCount() const =0;
  
  /// \brief Get the FieldDecl for the given child.
  ///
  virtual ::clang::FieldDecl const *getChildField(unsigned Index) const =0;
  
  /// \brief Get a child of this value.
  ///
  virtual std::shared_ptr<Value const> getChildAt(unsigned Index) const =0;
};


/// \brief Represents a pointer's runtime value.
///
class ValueOfPointer : public Value {
private:
  /// \brief Check if this is a valid opaque pointer (e.g. a DIR *).
  ///
  virtual bool isValidOpaqueImpl() const =0;
  
  /// \brief Get the raw value of this pointer.
  ///
  virtual stateptr_ty getRawValueImpl() const =0;
  
  /// \brief Get the size of the pointee type.
  ///
  virtual ::clang::CharUnits getPointeeSizeImpl() const =0;

public:
  /// \brief Constructor.
  ///
  ValueOfPointer()
  : Value(Value::Kind::Pointer)
  {}
  
  /// \brief Implement LLVM-style RTTI.
  ///
  static bool classof(Value const *V) {
    return V->getKind() == Value::Kind::Pointer;
  }
  
  /// \brief Get the highest legal dereference index of this value.
  ///
  virtual int getDereferenceIndexLimit() const =0;
  
  /// \brief Get the Value of this Value dereferenced.
  ///
  virtual std::shared_ptr<Value const> getDereferenced(int Index) const =0;
  
  /// \brief Check if this is a valid opaque pointer (e.g. a DIR *).
  ///
  bool isValidOpaque() const { return isValidOpaqueImpl(); }
  
  /// \brief Get the raw value of this pointer.
  ///
  stateptr_ty getRawValue() const { return getRawValueImpl(); }
  
  /// \brief Get the size of the pointee type.
  ///
  ::clang::CharUnits getPointeeSize() const { return getPointeeSizeImpl(); }
};


// Forward-declare for ValueStore.
class ValueStoreImpl;


/// \brief Ensures that in-memory Value objects are uniqued.
///
class ValueStore final {
  /// The underlying implementation.
  std::unique_ptr<ValueStoreImpl> Impl;
  
  /// \brief Constructor.
  ///
  ValueStore(seec::seec_clang::MappedModule const &WithMapping);
  
  // Don't allow copying or moving.
  ValueStore(ValueStore const &) = delete;
  ValueStore(ValueStore &&) = delete;
  ValueStore &operator=(ValueStore const &) = delete;
  ValueStore &operator=(ValueStore &&) = delete;
  
public:
  /// \brief Create a new ValueStore.
  ///
  static std::shared_ptr<ValueStore const>
  create(seec::seec_clang::MappedModule const &WithMapping) {
    return std::shared_ptr<ValueStore const>(new ValueStore(WithMapping));
  }
  
  /// \brief Destructor.
  ///
  ~ValueStore();
  
  /// \brief Access the underlying implementation.
  ///
  ValueStoreImpl const &getImpl() const;

  /// \brief Find a \c Value from an address and type string.
  ///
  std::shared_ptr<Value const>
  findFromAddressAndType(stateptr_ty Address, llvm::StringRef TypeString) const;
};


/// \name Value Creation
/// @{

/// \brief Get a Value for a given type in memory.
///
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         ::clang::QualType QualType,
         ::clang::ASTContext const &ASTContext,
         stateptr_ty Address,
         seec::trace::ProcessState const &ProcessState,
         seec::trace::FunctionState const *OwningFunction);


/// \brief Get a Value for a MappedStmt.
///
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         seec::seec_clang::MappedStmt const &MappedStatement,
         seec::trace::FunctionState const &FunctionState);


/// \brief Get a Value for a Stmt.
///
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         ::clang::Stmt const *Statement,
         seec::seec_clang::MappedModule const &Mapping,
         seec::trace::FunctionState const &FunctionState);

/// @} (Value Creation)


/// \name Utilities
/// @{

/// \brief Check if a Value is a contained child of another Value.
///
/// This function considers array elements and record members to be children,
/// but will not follow pointers.
///
bool isContainedChild(Value const &Child, Value const &Parent);

/// \brief Check if two pointers reference the same Value.
///
bool doReferenceSameValue(ValueOfPointer const &A, ValueOfPointer const &B);

/// \brief Visit a value and all of its direct descendents using a callback.
///
template<typename FnT>
void visitChildren(Value const &V, FnT &&Callback) {
  Callback(V);
  
  switch (V.getKind()) {
    case Value::Kind::Array:
    {
      auto const &A = llvm::cast<ValueOfArray>(V);
      auto const ChildCount = A.getChildCount();
      
      for (unsigned i = 0; i < ChildCount; ++i)
        if (auto const Child = A.getChildAt(i))
          visit(*Child, Callback);
      
      break;
    }
    
    case Value::Kind::Record:
    {
      auto const &R = llvm::cast<ValueOfRecord>(V);
      auto const ChildCount = R.getChildCount();
      
      for (unsigned i = 0; i < ChildCount; ++i)
        if (auto const Child = R.getChildAt(i))
          visit(*Child, Callback);
      
      break;
    }
    
    // The following values kinds do not have direct descendents.
    case Value::Kind::Basic:         SEEC_FALLTHROUGH;
    case Value::Kind::Complex:       SEEC_FALLTHROUGH;
    case Value::Kind::Scalar:        SEEC_FALLTHROUGH;
    case Value::Kind::Pointer:       break;
  }
}

/// \brief Search a value and all of its direct descendents for a value that
///        matches a predicate.
///
template<typename FnT>
bool searchChildren(Value const &V, FnT &&Predicate) {
  if (Predicate(V))
    return true;
  
  switch (V.getKind()) {
    case Value::Kind::Array:
    {
      auto const &A = llvm::cast<ValueOfArray>(V);
      auto const ChildCount = A.getChildCount();
      
      for (unsigned i = 0; i < ChildCount; ++i)
        if (auto const Child = A.getChildAt(i))
          if (searchChildren(*Child, Predicate))
            return true;
      
      break;
    }
    
    case Value::Kind::Record:
    {
      auto const &R = llvm::cast<ValueOfRecord>(V);
      auto const ChildCount = R.getChildCount();
      
      for (unsigned i = 0; i < ChildCount; ++i)
        if (auto const Child = R.getChildAt(i))
          if (searchChildren(*Child, Predicate))
            return true;
      
      break;
    }
    
    // The following values kinds do not have direct descendents.
    case Value::Kind::Basic:         SEEC_FALLTHROUGH;
    case Value::Kind::Scalar:        SEEC_FALLTHROUGH;
    case Value::Kind::Complex:       SEEC_FALLTHROUGH;
    case Value::Kind::Pointer:       break;
  }
  
  return false;
}

/// @} (Utilities)


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDVALUE_HPP
