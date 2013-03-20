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

#include <string>


namespace clang {
  class FunctionDecl;
  class Stmt;
}

namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace trace {
  class FunctionState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class AllocaState;
class ThreadState;


/// \brief SeeC-Clang-mapped function state.
///
class FunctionState {
  /// The thread that this function belongs to.
  ThreadState &Parent;
  
  /// The base (unmapped) state.
  seec::trace::FunctionState &UnmappedState;
  
  /// The mapped parameters.
  std::vector<AllocaState> Parameters;
  
  /// The mapped local variables.
  std::vector<AllocaState> Variables;
  
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
  
  /// \brief Get the FunctionDecl for this function.
  ///
  ::clang::FunctionDecl const *getFunctionDecl() const;
  
  /// \brief Get a name for this function.
  ///
  std::string getNameAsString() const;
  
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
  
  
  
  /// @} (Local variables.)
};


/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDFUNCTIONSTATE_HPP
