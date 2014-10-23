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

#include "seec/Clang/GraphExpansion.hpp"
#include "seec/Clang/MappedMallocState.hpp"
#include "seec/Clang/MappedFunctionState.hpp"
#include "seec/Clang/MappedGlobalVariable.hpp"
#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedThreadState.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/Trace/ProcessState.hpp"
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
  
  /// \brief Add the given pointer if it doesn't already exist.
  ///
  /// \return true iff the Pointer didn't already exist and was added.
  ///
  bool addPointer(std::shared_ptr<ValueOfPointer const> const &Pointer)
  {
    auto const Address = Pointer->getRawValue();
    
    for (auto const &Pair : range(Pointers.equal_range(Address)))
      if (Pair.second == Pointer)
        return false;
    
    Pointers.insert(std::make_pair(Address, Pointer));
    
    return true;
  }
  
  std::vector<Dereference>
  getReferencesOf(Value const &Val) const;
  
  std::vector<std::shared_ptr<ValueOfPointer const>>
  getReferencesOfArea(uintptr_t Start, uintptr_t End) const;
  
  bool isAreaReferenced(uintptr_t Start, uintptr_t End) const;
  
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

bool ExpansionImpl::isAreaReferenced(uintptr_t Start, uintptr_t End) const
{
  return Pointers.lower_bound(Start) != Pointers.lower_bound(End);
}


//===----------------------------------------------------------------------===//
// expand
//===----------------------------------------------------------------------===//

static bool containsPointerType(clang::Type const *CanonTy)
{
  if (!CanonTy)
    return false;
  
  if (CanonTy->isAnyPointerType())
    return true;
  
  if (auto const ArrayTy = llvm::dyn_cast<clang::ArrayType>(CanonTy)) {
    auto const ElemTy = ArrayTy->getElementType().getCanonicalType();
    return containsPointerType(ElemTy.getTypePtrOrNull());
  }
  else if (auto const RecordTy = llvm::dyn_cast<clang::RecordType>(CanonTy)) {
    auto const Decl = RecordTy->getDecl();
    
    for (auto Field : seec::range(Decl->field_begin(), Decl->field_end())) {
      auto const FieldTy = Field->getType().getCanonicalType();
      if (containsPointerType(FieldTy.getTypePtrOrNull()))
        return true;
    }
    
    return false;
  }
  
  return false;
}

void expand(ExpansionImpl &EI, std::shared_ptr<Value const> const &State)
{
  assert(State);
  
  switch (State->getKind()) {
    case seec::cm::Value::Kind::Basic: SEEC_FALLTHROUGH;
    case seec::cm::Value::Kind::Scalar:
      break;
    
    case seec::cm::Value::Kind::Array:
      {
        if (containsPointerType(State->getCanonicalType())) {
          auto const &Array = *llvm::cast<seec::cm::ValueOfArray>(State.get());
          unsigned const ChildCount = Array.getChildCount();
          
          for (unsigned i = 0; i < ChildCount; ++i)
            expand(EI, Array.getChildAt(i));
        }
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
        
        if (!EI.addPointer(Ptr)) // Pointer has already been expanded.
          break;
        
        // If the pointee contains a pointer type, then expand all possible
        // dereferences so that we generate a complete graph. Otherwise, only
        // expand the direct dereference.
        
        unsigned Limit = Ptr->getDereferenceIndexLimit();
        
        if (Limit > 1) {
          auto const CanonTy = Ptr->getCanonicalType();
          auto const PtrTy = llvm::cast<clang::PointerType>(CanonTy);
          auto const PointeeTy = PtrTy->getPointeeType().getTypePtrOrNull();
          if (!containsPointerType(PointeeTy))
            Limit = 1;
        }
        
        for (unsigned i = 0; i < Limit; ++i) {
          auto const Pointee = Ptr->getDereferenced(i);
          EI.addReference(*Pointee, Dereference{Ptr, i});
          expand(EI, Pointee);
        }
      }
      break;
  }
}

void expand(ExpansionImpl &EI, seec::cm::LocalState const &State)
{
  auto const Value = State.getValue();
  if (Value)
    expand(EI, Value);
}

void expand(ExpansionImpl &EI, seec::cm::ParamState const &State)
{
  auto const Value = State.getValue();
  if (Value)
    expand(EI, Value);
}

void expand(ExpansionImpl &EI, seec::cm::FunctionState const &State)
{
  for (auto const &Parameter : State.getParameters())
    expand(EI, Parameter);
  
  for (auto const &Local : State.getLocals())
    expand(EI, Local);
  
  if (auto const ActiveStmt = State.getActiveStmt())
    if (auto const Value = State.getStmtValue(ActiveStmt))
      expand(EI, Value);
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

bool
Expansion::isReferencedDirectly(Value const &Value) const
{
  for (auto const &Deref : Impl->getReferencesOf(Value))
    if (Deref.getIndex() == 0)
      return true;
  return false;
}

std::vector<std::shared_ptr<ValueOfPointer const>>
Expansion::getReferencesOfArea(uintptr_t Start, uintptr_t End) const
{
  return Impl->getReferencesOfArea(Start, End);
}

bool Expansion::isAreaReferenced(uintptr_t Start, uintptr_t End) const
{
  return Impl->isAreaReferenced(Start, End);
}

std::vector<std::shared_ptr<ValueOfPointer const>>
Expansion::getAllPointers() const
{
  return Impl->getAllPointers();
}


//===----------------------------------------------------------------------===//
// reduceReferences()
//===----------------------------------------------------------------------===//

static
bool
isChildOfAnyDereference(std::shared_ptr<Value const> const &Child,
                        std::shared_ptr<ValueOfPointer const> const &Ptr)
{
  auto const Limit = Ptr->getDereferenceIndexLimit();

  for (unsigned i = 0; i < Limit; ++i) {
    auto const Pointee = Ptr->getDereferenced(i);
    if (isContainedChild(*Child, *Pointee))
      return true;
  }

  return false;
}

void reduceReferences(std::vector<std::shared_ptr<ValueOfPointer const>> &Refs)
{
  typedef std::shared_ptr<ValueOfPointer const> ValOfPtr;

  // Move all the void pointers to the end of the list. If we have nothing but
  // void pointers then return, otherwise remove all of them.
  auto const VoidIt =
    std::partition(Refs.begin(), Refs.end(),
                  [] (ValOfPtr const &Ptr) -> bool {
                    auto const CanTy = Ptr->getCanonicalType();
                    auto const PtrTy = llvm::cast<clang::PointerType>(CanTy);
                    return !PtrTy->getPointeeType()->isVoidType();
                  });

  if (VoidIt == Refs.begin())
    return;

  Refs.erase(VoidIt, Refs.end());
  if (Refs.size() == 1)
    return;

  // Remove all pointers to incomplete types to the end of the list. If we have
  // nothing but pointers to incomplete types, then return.
  auto const IncompleteIt =
    std::partition(Refs.begin(), Refs.end(),
                    [] (ValOfPtr const &Ptr) -> bool {
                      auto const CanTy = Ptr->getCanonicalType();
                      auto const PtrTy = llvm::cast<clang::PointerType>(CanTy);
                      return !PtrTy->getPointeeType()->isIncompleteType();
                    });

  if (IncompleteIt == Refs.begin())
    return;

  Refs.erase(IncompleteIt, Refs.end());
  if (Refs.size() == 1)
    return;

  // Remove all references which refer to a child of another reference. E.g. if
  // we have a pointer to a struct, and a pointer to a member of that struct,
  // then we should remove the member pointer (if the struct is selected for
  // layout then the pointer will be rendered correctly, otherwise it will be
  // rendered as punned).
  //
  // Also remove references which refer either to the same value as another
  // reference. If one of these references has a lower raw value then it should
  // be kept, as it will have more dereferences (and thus a more complete
  // layout will be produced using it).
  auto const CurrentRefs = Refs;
  auto const RemovedIt =
    std::remove_if(Refs.begin(), Refs.end(),
      [&] (ValOfPtr const &Ptr) -> bool
      {
        auto const Pointee = Ptr->getDereferenced(0);
        return std::any_of(CurrentRefs.begin(), CurrentRefs.end(),
                  [&] (ValOfPtr const &Other) -> bool {
                    if (Ptr == Other)
                      return false;

                    // Check if we directly reference another pointer's
                    // dereference. If so, don't bother checking children, as
                    // either us or the other pointer will be removed anyway.
                    auto const Direct = doReferenceSameValue(*Ptr, *Other);
                    if (Direct)
                      return Ptr->getRawValue() > Other->getRawValue();

                    // Check if this pointer directly references a child.
                    return isChildOfAnyDereference(Pointee, Other);
                  });
      });

  if (RemovedIt != Refs.begin())
    Refs.erase(RemovedIt, Refs.end());
}


} // namespace graph (in cm in seec)

} // namespace cm (in seec)

} // namespace seec
