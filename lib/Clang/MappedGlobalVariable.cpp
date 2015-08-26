//===- lib/Clang/MappedGlobalVariable.cpp ---------------------------------===//
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

#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"

#include "clang/AST/Decl.h"

#include "llvm/Support/raw_ostream.h"


namespace seec {

// Documented in MappedProcessTrace.hpp
namespace cm {


//===----------------------------------------------------------------------===//
// GlobalVariable
//===----------------------------------------------------------------------===//

::clang::ValueDecl const *GlobalVariable::getClangValueDecl() const
{
  return Mapping.getDecl();
}

::llvm::GlobalVariable const *GlobalVariable::getLLVMGlobalVariable() const
{
  return Mapping.getGlobal();
}

bool GlobalVariable::isInSystemHeader() const
{
  return Mapping.isInSystemHeader();
}

bool GlobalVariable::isReferenced() const
{
  return Mapping.isReferenced();
}

std::shared_ptr<Value const> GlobalVariable::getValue() const
{
  auto const Decl = Mapping.getDecl();
  
  return seec::cm::getValue(State.getCurrentValueStore(),
                            Decl->getType(),
                            Decl->getDeclContext()->getParentASTContext(),
                            Address,
                            State.getUnmappedProcessState(),
                            /* OwningFunction */ nullptr);
}


//===----------------------------------------------------------------------===//
// GlobalVariable: Printing
//===----------------------------------------------------------------------===//

// Documented in MappedGlobalVariable.hpp
//
llvm::raw_ostream &operator<<(llvm::raw_ostream &Out,
                              GlobalVariable const &State)
{
  Out << State.getClangValueDecl()->getName() << " = ";
  
  auto const Value = State.getValue();
  if (Value)
    Out << Value->getValueAsStringShort();
  else
    Out << "<couldn't get value>";
  
  return Out;
}


} // namespace cm (in seec)

} // namespace seec
