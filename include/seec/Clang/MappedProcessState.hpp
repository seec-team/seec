//===- include/seec/Clang/MappedProcessState.hpp --------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDPROCESSSTATE_HPP
#define SEEC_CLANG_MAPPEDPROCESSSTATE_HPP

#include "seec/Clang/MappedProcessTrace.hpp"

#include <memory>


namespace seec {

namespace trace {
  class ProcessState;
}

// Documented in MappedProcessTrace.hpp
namespace cm {


/// \brief SeeC-Clang-mapped process state.
///
class ProcessState {
  /// The SeeC-Clang-mapped trace.
  seec::cm::ProcessTrace const &Trace;
  
  /// The base (unmapped) state.
  std::unique_ptr<seec::trace::ProcessState> UnmappedState;
  
public:
  /// \brief Constructor.
  ///
  ProcessState(seec::cm::ProcessTrace const &Trace);
  
  /// \brief Move constructor.
  ///
  ProcessState(ProcessState &&) = default;
  
  // No copy constructor.
  ProcessState(ProcessState const &Other) = delete;
  
  /// \brief Move assignment.
  ///
  ProcessState &operator=(ProcessState &&) = default;
  
  // No copy assignment.
  ProcessState &operator=(ProcessState const &RHS) = delete;
  
  /// \brief Destructor.
  ///
  ~ProcessState();
};


} // namespace cm (in seec)

} // namespace seec

#endif // SEEC_CLANG_MAPPEDPROCESSSTATE_HPP
