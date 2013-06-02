//===- include/seec/Clang/MappedFunctionState.hpp -------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDFUNCTIONSTATE_HPP
#define SEEC_CLANG_MAPPEDFUNCTIONSTATE_HPP

#include "seec/Clang/MappedValue.hpp"
#include "seec/Util/Range.hpp"

#include <string>


namespace clang {
  class FunctionDecl;
  class Stmt;
}

namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace seec_clang {
  class MappedAST;
  class MappedFunctionDecl;
} // namespace seec_clang (in seec)

namespace trace {
  class FunctionState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class AllocaState;
class RuntimeErrorState;
class ThreadState;


/// \brief SeeC-Clang-mapped function state.
///
class FunctionState {
  /// The thread that this function belongs to.
  ThreadState &Parent;
  
  /// The base (unmapped) state.
  seec::trace::FunctionState &UnmappedState;
  
  /// The mapping information for this function.
  seec::seec_clang::MappedFunctionDecl const *Mapping;
  
  /// The mapped parameters.
  std::vector<AllocaState> Parameters;
  
  /// The mapped local variables.
  std::vector<AllocaState> Variables;
  
  /// The mapped runtime errors.
  std::vector<RuntimeErrorState> RuntimeErrors;
  
public:
  /// \brief Constructor.
  ///
  FunctionState(ThreadState &WithParent,
                seec::trace::FunctionState &ForUnmappedState);
  
  /// \brief Destructor.
  ///
  ~FunctionState();
  
  /// \brief Move constructor.
  ///
  FunctionState(FunctionState &&) = default;
  
  /// \brief Move assignment.
  ///
  FunctionState &operator=(FunctionState &&) = default;
  
  // No copying.
  FunctionState(FunctionState const &) = delete;
  FunctionState &operator=(FunctionState const &) = delete;
  
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation) const;
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the underlying (unmapped) state.
  ///
  seec::trace::FunctionState &getUnmappedState() { return UnmappedState; }
  
  /// \brief Get the underlying (unmapped) state.
  ///
  seec::trace::FunctionState const &getUnmappedState() const {
    return UnmappedState;
  }
  
  /// @} (Access underlying information.)
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the ThreadState that this FunctionState belongs to.
  ///
  ThreadState &getParent() { return Parent; }
  
  /// \brief Get the ThreadState that this FunctionState belongs to.
  ///
  ThreadState const &getParent() const { return Parent; }
  
  /// \brief Get the FunctionDecl for this function.
  ///
  ::clang::FunctionDecl const *getFunctionDecl() const;
  
  /// \brief Get a name for this function.
  ///
  std::string getNameAsString() const;
  
  /// \brief Get the mapped AST that this function belongs to.
  ///
  seec::seec_clang::MappedAST const *getMappedAST() const;
  
  /// @} (Accessors.)
  
  
  /// \name Stmt evaluation.
  /// @{
  
  /// \brief Get the active Stmt (if any).
  ///
  /// This Stmt may have just finished evaluating, or may be partially
  /// evaluated.
  ///
  ::clang::Stmt const *getActiveStmt() const;
  
  /// \brief Get the Value of a Stmt.
  ///
  /// This gets the Value resulting from the last evaluation of the given Stmt,
  /// if it has been evaluated (and the value is mapped).
  ///
  std::shared_ptr<Value const> getStmtValue(::clang::Stmt const *S) const;
  
  /// @} (Stmt evaluation.)
  
  
  /// \name Local variables.
  /// @{
  
  /// \brief Get the mapped parameters.
  std::vector<AllocaState> const &getParameters() const {
    return Parameters;
  }
  
  /// \brief Get the mapped local variables.
  std::vector<AllocaState> const &getLocals() const {
    return Variables;
  }
  
  /// @} (Local variables.)
  
  
  /// \name Runtime Errors.
  /// @{
  
  /// \brief Get all runtime errors.
  ///
  decltype(RuntimeErrors) const &getRuntimeErrors() const {
    return RuntimeErrors;
  }
  
  /// \brief Get active runtime errors.
  ///
  seec::Range<decltype(RuntimeErrors)::const_iterator>
  getRuntimeErrorsActive() const;
  
  /// @} (Runtime Errors.)
};


/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDFUNCTIONSTATE_HPP
