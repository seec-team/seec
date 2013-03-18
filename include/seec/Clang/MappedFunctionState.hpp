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

#include <string>


namespace clang {
  class FunctionDecl;
}

namespace llvm {
  class raw_ostream;
} // namespace llvm

namespace seec {

namespace trace {
  class FunctionState;
} // namespace trace (in seec)

// Documented in MappedProcessTrace.hpp
namespace cm {

class ThreadState;


/// \brief SeeC-Clang-mapped function state.
///
class FunctionState {
  /// The thread that this function belongs to.
  ThreadState &Parent;
  
  /// The base (unmapped) state.
  seec::trace::FunctionState &UnmappedState;
  
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
  
  
  /// \name Access underlying information.
  /// @{
  
  /// \brief Get the underlying (unmapped) state.
  ///
  decltype(UnmappedState) &getUnmappedState() { return UnmappedState; }
  
  /// \brief Get the underlying (unmapped) state.
  ///
  decltype(UnmappedState) const &getUnmappedState() const {
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
};


/// Print a textual description of a ThreadState.
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              FunctionState const &State);


} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDFUNCTIONSTATE_HPP
