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

public:
  /// \brief Constructor.
  ///
  AllocaState(FunctionState &WithParent,
              seec::trace::AllocaState &ForUnmappedState,
              clang::VarDecl const *Decl);
  
  /// \brief Print a textual description of the state.
  ///
  void print(llvm::raw_ostream &Out,
             seec::util::IndentationGuide &Indentation) const;
};


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDALLOCASTATE_HPP
