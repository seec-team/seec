//===- include/seec/Clang/MappedMallocState.hpp ---------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDMALLOCSTATE_HPP
#define SEEC_CLANG_MAPPEDMALLOCSTATE_HPP


#include "seec/Clang/MappedModule.hpp"
#include "seec/Clang/MappedStateCommon.hpp"

#include <cstddef>
#include <cstdint>


namespace clang {
  class Stmt;
}


namespace llvm {
  class Instruction;
  class raw_ostream;
} // namespace llvm


namespace seec {

namespace trace {
  class MallocState;
} // namespace trace (in seec)

namespace util {
  class IndentationGuide;
} // namespace util (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class ProcessState;


/// \brief A SeeC-Clang Mapped dynamic memory allocation.
///
class MallocState {
  /// The process state that this belongs to.
  ProcessState const &Parent;
  
  /// The base (unmapped) state.
  seec::trace::MallocState const &UnmappedState;
  
public:
  /// \brief Constructor.
  ///
  MallocState(ProcessState const &WithParent,
              seec::trace::MallocState const &ForState);
  
  /// \brief Get the runtime address of this allocation.
  ///
  stateptr_ty getAddress() const;
  
  /// \brief Get the number of bytes allocated.
  ///
  std::size_t getSize() const;
  
  /// \brief Get the llvm::Instruction that caused this allocation.
  ///
  llvm::Instruction const *getAllocatorInst() const;
  
  /// \brief Get mapping for the llvm::Instruction that caused this allocation.
  ///
  seec::seec_clang::MappedInstruction getAllocatorInstMapping() const;
  
  /// \brief Get the clang::Stmt that caused this allocation.
  ///
  clang::Stmt const *getAllocatorStmt() const;
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation) const;
};


/// \brief Print a textual description of a MallocState.
///
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              MallocState const &State);


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDMALLOCSTATE_HPP
