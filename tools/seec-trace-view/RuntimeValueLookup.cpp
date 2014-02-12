//===- tools/seec-trace-view/RuntimeValueLookup.cpp -----------------------===//
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

#include "seec/Clang/MappedFunctionState.hpp"

#include "RuntimeValueLookup.hpp"


bool
RuntimeValueLookupForFunction::
isValueAvailableForImpl(::clang::Stmt const *Statement) const
{
  if (!m_Function)
    return false;
  
  return m_Function->getStmtValue(Statement) ? true : false;
}

std::string
RuntimeValueLookupForFunction::
getValueStringImpl(::clang::Stmt const *Statement) const
{
  if (!m_Function)
    return std::string();
  
  auto const Value = m_Function->getStmtValue(Statement);
  if (!Value)
    return std::string();
  
  return Value->getValueAsStringFull();
}

seec::Maybe<bool>
RuntimeValueLookupForFunction::
getValueAsBoolImpl(::clang::Stmt const *Statement) const
{
  if (!m_Function)
    return seec::Maybe<bool>();
  
  auto const Value = m_Function->getStmtValue(Statement);
  if (!Value || !Value->isCompletelyInitialized()
      || !llvm::isa<seec::cm::ValueOfScalar>(*Value))
    return seec::Maybe<bool>();
  
  auto &Scalar = llvm::cast<seec::cm::ValueOfScalar>(*Value);
  return !Scalar.isZero();
}
