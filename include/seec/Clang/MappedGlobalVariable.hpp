//===- include/seec/Clang/MappedGlobalVariable.hpp ------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDGLOBALVARIABLE_HPP
#define SEEC_CLANG_MAPPEDGLOBALVARIABLE_HPP

#include "seec/Clang/MappedValue.hpp"

#include <cstdint>
#include <memory>
#include <vector>


namespace clang {
  class ValueDecl;
} // namespace clang

namespace llvm {
  class raw_ostream;
  class GlobalVariable;
} // namespace llvm


namespace seec {

namespace seec_clang {
  class MappedGlobalVariableDecl;
} // namespace seec_clang

// Documented in MappedProcessTrace.hpp
namespace cm {


class ProcessState;


/// \brief A SeeC-Clang-mapped global variable.
///
class GlobalVariable {
  /// The process state that this global variable belongs to.
  ProcessState const &State;
  
  /// All mapping information for this global variable.
  seec_clang::MappedGlobalVariableDecl const &Mapping;
  
  /// The run-time address.
  uintptr_t Address;
  
public:
  /// \brief Constructor.
  ///
  GlobalVariable(ProcessState const &ForState,
                 seec_clang::MappedGlobalVariableDecl const &WithMapping,
                 uintptr_t WithAddress)
  : State(ForState),
    Mapping(WithMapping),
    Address(WithAddress)
  {}
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the clang::ValueDecl for this global.
  ///
  ::clang::ValueDecl const *getClangValueDecl() const;
  
  /// \brief Get the llvm::GlobalVariable for this global.
  ///
  ::llvm::GlobalVariable const *getLLVMGlobalVariable() const;
  
  /// \brief Get the run-time address of this global.
  ///
  uintptr_t getAddress() const { return Address; }
  
  /// @} (Accessors)
  
  
  /// \name Queries.
  /// @{
  
  /// \brief Check if this global is declared in a system header.
  ///
  bool isInSystemHeader() const;
  
  /// \brief Check if this global variable is referenced by user code.
  ///
  bool isReferenced() const;
  
  /// \brief Get the current run-time value of this global.
  ///
  std::shared_ptr<Value const> getValue() const;
  
  /// @} (Queries)
};


/// \brief Print a textual description of a GlobalVariable.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              GlobalVariable const &State);


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDGLOBALVARIABLE_HPP
