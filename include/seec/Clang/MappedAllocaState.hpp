//===- include/seec/Clang/MappedAllocaState.hpp ---------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDALLOCASTATE_HPP
#define SEEC_CLANG_MAPPEDALLOCASTATE_HPP


#include "seec/Clang/MappedValue.hpp"


namespace clang {
  class VarDecl;
} // namespace clang

namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace trace {
  class AllocaState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class FunctionState;


/// \brief SeeC-Clang-mapped alloca state.
///
class AllocaState {
  /// The function state that this alloca belongs to.
  FunctionState &Parent;
  
  /// The base (unmapped) state.
  seec::trace::AllocaState const &UnmappedState;
  
  /// The mapped Decl.
  ::clang::VarDecl const *Decl;
  
public:
  /// \brief Constructor.
  ///
  AllocaState(FunctionState &WithParent,
              seec::trace::AllocaState const &ForUnmappedState,
              ::clang::VarDecl const *ForDecl);
  
  /// \brief Destructor.
  ///
  ~AllocaState();
  
  /// \brief Move constructor.
  ///
  AllocaState(AllocaState &&) = default;
  
  /// \brief Move assignment.
  ///
  AllocaState &operator=(AllocaState &&) = default;
  
  // No copying.
  AllocaState(AllocaState const &) = delete;
  AllocaState &operator=(AllocaState const &) = delete;
  
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation) const;
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the unmapped state for this alloca.
  ///
  seec::trace::AllocaState const &getUnmappedState() const {
    return UnmappedState;
  }
  
  /// @} (Access underlying information.)
  
  
  /// \name Accessors.
  /// @{
  
  /// \brief Get the VarDecl for this alloca.
  ///
  ::clang::VarDecl const *getDecl() const { return Decl; }
  
  /// \brief Get the current mapped Value of this alloca.
  ///
  std::shared_ptr<Value const> getValue() const;
  
  /// @} (Accessors.)
};


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDALLOCASTATE_HPP
