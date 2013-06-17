//===- lib/Clang/GraphExpansion.cpp ---------------------------------------===//
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

#include "GraphExpansion.hpp"

#include "seec/Clang/MappedAllocaState.hpp"
#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/Range.hpp"

#include <map>


namespace seec {

namespace cm {

namespace graph {


//===----------------------------------------------------------------------===//
// ExpansionImpl
//===----------------------------------------------------------------------===//

class ExpansionImpl final {
  /// Map from Value to pointer dereferences that reference that Value.
  ///
  std::multimap<Value const *, Dereference> Edges;
  
  /// Map from address to pointers that reference that address.
  ///
  std::multimap<uintptr_t, std::shared_ptr<ValueOfPointer const>> Pointers;
  
public:
  ExpansionImpl()
  : Edges(),
    Pointers()
  {}
  
  void addReference(Value const &ToValue, Dereference FromPointer)
  {
    Edges.insert(std::make_pair(&ToValue, std::move(FromPointer)));
  }
  
  void addPointer(std::shared_ptr<ValueOfPointer const> Pointer)
  {
    Pointers.insert(std::make_pair(Pointer->getRawValue(),
                                   std::move(Pointer)));
  }
  
  std::size_t countReferencesOf(Value const &Val) const {
    return Edges.count(&Val);
  }
  
  std::vector<Dereference>
  getReferencesOf(Value const &Val) const;
  
  std::vector<std::shared_ptr<ValueOfPointer const>>
  getReferencesOfArea(uintptr_t Start, uintptr_t End) const;
  
  std::vector<std::shared_ptr<ValueOfPointer const>>
  getAllPointers() const {
    std::vector<std::shared_ptr<ValueOfPointer const>> Ret;
    
    for (auto const &Pair : Pointers)
      Ret.emplace_back(Pair.second);
    
    return Ret;
  }
};

std::vector<Dereference>
ExpansionImpl::getReferencesOf(Value const &Val) const
{
  std::vector<Dereference> Ret;
  
  for (auto const &Pair : seec::range(Edges.equal_range(&Val)))
    Ret.emplace_back(Pair.second);
  
  return Ret;
}

std::vector<std::shared_ptr<ValueOfPointer const>>
ExpansionImpl::getReferencesOfArea(uintptr_t Start, uintptr_t End) const
{
  auto const Range = seec::range(Pointers.lower_bound(Start),
                                 Pointers.lower_bound(End));
  
  std::vector<std::shared_ptr<ValueOfPointer const>> Ret;
  
  for (auto const &Pair : Range)
    Ret.emplace_back(Pair.second);
  
  return Ret;
}


//===----------------------------------------------------------------------===//
// expand
//===----------------------------------------------------------------------===//

void expand(ExpansionImpl &EI, std::shared_ptr<Value const> const &State)
{
  assert(State);
  
  switch (State->getKind()) {
    case seec::cm::Value::Kind::Basic: SEEC_FALLTHROUGH;
    case seec::cm::Value::Kind::Scalar:
      break;
    
    case seec::cm::Value::Kind::Array:
      {
        auto const &Array = *llvm::cast<seec::cm::ValueOfArray>(State.get());
        unsigned const ChildCount = Array.getChildCount();
        
        for (unsigned i = 0; i < ChildCount; ++i)
          expand(EI, Array.getChildAt(i));
      }
      break;
    
    case seec::cm::Value::Kind::Record:
      {
        auto const &Record = *llvm::cast<seec::cm::ValueOfRecord>(State.get());
        unsigned const ChildCount = Record.getChildCount();
        
        for (unsigned i = 0; i < ChildCount; ++i)
          expand(EI, Record.getChildAt(i));
      }
      break;
    
    case seec::cm::Value::Kind::Pointer:
      {
        auto const Ptr =
          std::static_pointer_cast<seec::cm::ValueOfPointer const>(State);
        
        unsigned const Limit = Ptr->getDereferenceIndexLimit();
        
        EI.addPointer(Ptr);
        
        for (unsigned i = 0; i < Limit; ++i) {
          auto const Pointee = Ptr->getDereferenced(i);
          EI.addReference(*Pointee, Dereference{Ptr, i});
          expand(EI, Pointee);
        }
      }
      break;
  }
}

void expand(ExpansionImpl &EI, seec::cm::AllocaState const &State)
{
  expand(EI, State.getValue());
}

void expand(ExpansionImpl &EI, seec::cm::ParamState const &State)
{
  expand(EI, State.getValue());
}

void expand(ExpansionImpl &EI, seec::cm::FunctionState const &State)
{
  for (auto const &Parameter : State.getParameters())
    expand(EI, Parameter);
  
  for (auto const &Local : State.getLocals())
    expand(EI, Local);
}

void expand(ExpansionImpl &EI, seec::cm::ThreadState const &State)
{
  for (auto const &FunctionState : State.getCallStack())
    expand(EI, FunctionState);
}

void expand(ExpansionImpl &EI, seec::cm::GlobalVariable const &State)
{
  expand(EI, State.getValue());
}

void expand(ExpansionImpl &EI, seec::cm::ProcessState const &State)
{
  for (std::size_t i = 0; i < State.getThreadCount(); ++i)
    expand(EI, State.getThread(i));
  
  for (auto const &Global : State.getGlobalVariables())
    expand(EI, *Global);
}


//===----------------------------------------------------------------------===//
// Expansion
//===----------------------------------------------------------------------===//

Expansion::Expansion()
: Impl()
{}

Expansion::~Expansion() = default;

Expansion Expansion::from(seec::cm::ProcessState const &State)
{
  std::unique_ptr<ExpansionImpl> EI {new ExpansionImpl()};
  
  expand(*EI, State);
  
  Expansion E;
  
  E.Impl = std::move(EI);
  
  return E;
}

std::size_t
Expansion::countReferencesOf(std::shared_ptr<Value const> const &Value) const
{
  return Impl->countReferencesOf(*Value);
}

std::vector<Dereference>
Expansion::getReferencesOf(std::shared_ptr<Value const> const &Value) const
{
  return Impl->getReferencesOf(*Value);
}

std::vector<std::shared_ptr<ValueOfPointer const>>
Expansion::getReferencesOfArea(uintptr_t Start, uintptr_t End) const
{
  return Impl->getReferencesOfArea(Start, End);
}

std::vector<std::shared_ptr<ValueOfPointer const>>
Expansion::getAllPointers() const
{
  return Impl->getAllPointers();
}


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec
