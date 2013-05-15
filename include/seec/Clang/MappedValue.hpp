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


#include "clang/AST/CharUnits.h"
#include "clang/AST/Type.h"

#include "llvm/Support/Casting.h"

#include <memory>
#include <string>


namespace clang {
  class ASTContext;
  class Expr;
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


/// \brief Represents a runtime value.
///
class Value {
public:
  enum class Kind {
    Basic,
    Array,
    Record,
    Pointer
  };

private:
  /// General Kind of this Value.
  Kind ThisKind;

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
  virtual uintptr_t getAddress() const =0;
  
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


/// \brief Represents an aggregate's runtime value.
///
class ValueOfArray : public Value {
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
  /// \brief Get the raw value of this pointer.
  ///
  virtual uintptr_t getRawValueImpl() const =0;
  
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
  virtual unsigned getDereferenceIndexLimit() const =0;
  
  /// \brief Get the Value of this Value dereferenced.
  ///
  virtual std::shared_ptr<Value const> getDereferenced(unsigned Index) const =0;
  
  /// \brief Get the raw value of this pointer.
  ///
  uintptr_t getRawValue() const { return getRawValueImpl(); }
  
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
  ValueStore();
  
  // Don't allow copying or moving.
  ValueStore(ValueStore const &) = delete;
  ValueStore(ValueStore &&) = delete;
  ValueStore &operator=(ValueStore const &) = delete;
  ValueStore &operator=(ValueStore &&) = delete;
  
public:
  /// \brief Create a new ValueStore.
  ///
  static std::shared_ptr<ValueStore const> create() {
    return std::shared_ptr<ValueStore const>(new ValueStore());
  }
  
  /// \brief Destructor.
  ///
  ~ValueStore();
  
  /// \brief Access the underlying implementation.
  ///
  ValueStoreImpl const &getImpl() const;
};


/// \brief Get a Value for a given type in memory.
///
std::shared_ptr<Value const>
getValue(std::shared_ptr<ValueStore const> Store,
         ::clang::QualType QualType,
         ::clang::ASTContext const &ASTContext,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState);


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


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDVALUE_HPP
