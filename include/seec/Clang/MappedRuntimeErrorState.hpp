//===- include/seec/Clang/MappedRuntimeErrorState.hpp ---------------------===//
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

#ifndef SEEC_CLANG_MAPPEDRUNTIMEERRORSTATE_HPP
#define SEEC_CLANG_MAPPEDRUNTIMEERRORSTATE_HPP

#include "seec/RuntimeErrors/UnicodeFormatter.hpp"


namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace runtime_errors {
  class ArgParameter;
  class RunError;
} // namespace runtime_errors (in seec)

namespace trace {
  class RuntimeErrorState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class FunctionState;


/// \brief Represents a SeeC-Clang mapped runtime error.
///
class RuntimeErrorState {
  /// The function state that this runtime error belongs to.
  FunctionState &Parent;
  
  /// The raw (unmapped) state.
  seec::trace::RuntimeErrorState const &UnmappedState;
  
public:
  /// \brief Constructor.
  ///
  RuntimeErrorState(FunctionState &WithParent,
                    seec::trace::RuntimeErrorState const &ForUnmappedState);
  
  // Moving.
  RuntimeErrorState(RuntimeErrorState &&) = default;
  RuntimeErrorState &operator=(RuntimeErrorState &&) = default;
  
  // No copying.
  RuntimeErrorState(RuntimeErrorState const &) = delete;
  RuntimeErrorState &operator=(RuntimeErrorState const &) = delete;
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation) const;
  
  /// \brief Get the raw (unmapped) state.
  ///
  seec::trace::RuntimeErrorState const &getUnmappedState() const {
    return UnmappedState;
  }
  
  /// \brief Get the RunError object.
  ///
  seec::runtime_errors::RunError const &getRunError() const;
  
  /// \brief Get a Description for this runtime error.
  ///
  seec::Maybe<std::unique_ptr<seec::runtime_errors::Description>,
              seec::Error>
  getDescription() const;
  
  /// \brief Get the Decl that owns the Instruction that caused this error.
  ///
  clang::Decl const *getDecl() const;

  /// \brief Get the Stmt that owns the Instruction that caused this error.
  ///
  clang::Stmt const *getStmt() const;
  
  /// \brief Get the Expr for the given parameter (if possible).
  ///
  clang::Expr const *
  getParameter(seec::runtime_errors::ArgParameter const &Param) const;
  
  
  /// \name Queries
  /// @{
  
  /// \brief Check if this runtime error is currently active.
  ///
  bool isActive() const;
  
  /// @} (Queries.)
};


/// Print a textual description of a RuntimeErrorState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              RuntimeErrorState const &State);


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDRUNTIMEERRORSTATE_HPP
