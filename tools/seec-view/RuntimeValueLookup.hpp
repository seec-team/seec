//===- tools/seec-trace-view/RuntimeValueLookup.hpp -----------------------===//
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

#ifndef SEEC_TRACE_VIEW_RUNTIMEVALUELOOKUP_HPP
#define SEEC_TRACE_VIEW_RUNTIMEVALUELOOKUP_HPP

#include "seec/ClangEPV/ClangEPV.hpp"
#include "seec/Util/Maybe.hpp"

#include <string>


namespace clang {
  class Stmt;
}

namespace seec {
  namespace cm {
    class FunctionState;
  }
}


/// \brief Implements \c RuntimeValueLookup from a \c seec::cm::FunctionState.
///
class RuntimeValueLookupForFunction : public seec::clang_epv::RuntimeValueLookup
{
  /// The \c FunctionState to retrieve values from.
  seec::cm::FunctionState const * const m_Function;
  
  /// \brief Check if a value is available for a Statement.
  ///
  virtual bool isValueAvailableForImpl(::clang::Stmt const *S) const override;
  
  /// \brief Get a string describing the current runtime value of Statement.
  ///
  virtual std::string getValueStringImpl(::clang::Stmt const *S) const override;
  
  /// \brief Check if a value is considered to be true, if possible.
  ///
  /// pre: isValueAvailableFor(Statement) == true
  ///
  virtual seec::Maybe<bool>
  getValueAsBoolImpl(::clang::Stmt const *S) const override;
  
public:
  /// \brief Constructor.
  ///
  RuntimeValueLookupForFunction(seec::cm::FunctionState const *Function)
  : m_Function(Function)
  {}
  
  /// \brief Destructor.
  ///
  virtual ~RuntimeValueLookupForFunction() override = default;
};

#endif // SEEC_TRACE_VIEW_RUNTIMEVALUELOOKUP_HPP
