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


#include "clang/AST/Type.h"

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


class ValueStoreImpl;


/// \brief Ensures that in-memory Value objects are uniqued.
///
class ValueStore {
  /// The underlying implementation.
  std::unique_ptr<ValueStoreImpl> Impl;
  
public:
  /// \brief Constructor.
  ///
  ValueStore();
  
  /// \brief Destructor.
  ///
  ~ValueStore();
  
  /// \brief Access the underlying implementation.
  ///
  ValueStoreImpl const &getImpl() const;
  
  /// \brief Free up unused memory.
  ///
  void freeUnused() const;
};


/// \brief Represents a runtime value.
///
class Value {
public:
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
  
  /// \brief Get the number of children of this value.
  ///
  virtual unsigned getChildCount() const =0;
  
  /// \brief Get a child of this value (if it is an aggregate type).
  ///
  virtual std::shared_ptr<Value const> getChildAt(unsigned Index) const =0;
  
  /// \brief Get the highest legal dereference index of this value.
  ///
  virtual unsigned getDereferenceIndexLimit() const =0;
  
  /// \brief Get the Value of this Value dereferenced (if it is a pointer type).
  ///
  virtual std::shared_ptr<Value const> getDereferenced(unsigned Index) const =0;
};


/// \brief Get a Value for a given type in memory.
///
std::shared_ptr<Value const>
getValue(::clang::QualType QualType,
         ::clang::ASTContext const &ASTContext,
         uintptr_t Address,
         seec::trace::ProcessState const &ProcessState);


/// \brief Get a Value for a MappedStmt.
///
std::shared_ptr<Value const>
getValue(seec::seec_clang::MappedStmt const &MappedStatement,
         seec::trace::FunctionState const &FunctionState);


/// \brief Get a Value for a Stmt.
///
std::shared_ptr<Value const>
getValue(::clang::Stmt const *Statement,
         seec::seec_clang::MappedModule const &Mapping,
         seec::trace::FunctionState const &FunctionState);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDVALUE_HPP
