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
/// This file defines ProcessState, a class for viewing the recorded states of
/// SeeC-Clang mapped processes. 
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

// Documented in MappedProcessTrace.hpp
namespace cm {


class ProcessState;


/// \brief A SeeC-Clang-mapped global variable.
///
class GlobalVariable {
  /// The process state that this global variable belongs to.
  ProcessState const &State;
  
  /// The Decl for the global variable.
  ::clang::ValueDecl const *Decl;
  
  /// The GlobalVariable for this Decl.
  ::llvm::GlobalVariable const *GV;
  
  /// The run-time address.
  uintptr_t Address;
  
public:
  /// \brief Constructor.
  ///
  GlobalVariable(ProcessState const &ForState,
                 ::clang::ValueDecl const *ForDecl,
                 ::llvm::GlobalVariable const *ForLLVMGlobalVariable,
                 uintptr_t WithAddress)
  : State(ForState),
    Decl(ForDecl),
    GV(ForLLVMGlobalVariable),
    Address(WithAddress)
  {}
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the clang::ValueDecl for this global.
  ///
  ::clang::ValueDecl const *getClangValueDecl() const { return Decl; }
  
  /// \brief Get the llvm::GlobalVariable for this global.
  ///
  ::llvm::GlobalVariable const *getLLVMGlobalVariable() const { return GV; }
  
  /// \brief Get the run-time address of this global.
  ///
  uintptr_t getAddress() const { return Address; }
  
  /// @}
  
  
  /// \brief Get the current run-time value of this global.
  ///
  std::shared_ptr<Value const> getValue() const;
};


/// \brief Print a textual description of a GlobalVariable.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              GlobalVariable const &State);


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDGLOBALVARIABLE_HPP
