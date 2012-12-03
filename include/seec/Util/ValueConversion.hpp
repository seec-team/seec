//===- ValueConversion.hpp ------------------------------------------ C++ -===//
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

#ifndef _SEEC_UTIL_VALUE_CONVERSION_HPP_
#define _SEEC_UTIL_VALUE_CONVERSION_HPP_

#include "llvm/Constants.h"
#include "llvm/Value.h"
#include "llvm/Type.h"
#include "llvm/TypeBuilder.h"

using namespace llvm;

namespace seec {

/// Enumerates possible positions to insert a new instruction relative to
/// another instruction.
enum class InsertPosition {
  Before, ///< Insert before the existing Instruction.
  After ///< Insert after the existing Instruction.
};

/// Insert an Instruction relative to an existing Instruction.
template<InsertPosition IP>
void insertInstruction(Instruction *I, Instruction *RelativeTo) {
  switch(IP) {
    case InsertPosition::Before:
      I->insertBefore(RelativeTo);
      break;
    case InsertPosition::After:
      I->insertAfter(RelativeTo);
      break;
  }
}

template<InsertPosition IP>
class GetValueAsTypeImpl {
public:
  static Value *getValueAs(Value *V, Type *Ty, Instruction *InsertAt=nullptr) {
    if (V->getType() == Ty)
      return V;

    if (PointerType *PTy = dyn_cast<PointerType>(Ty))
      return getValueAs(V, PTy, InsertAt);

    if (IntegerType *ITy = dyn_cast<IntegerType>(Ty))
      return getValueAs(V, ITy, InsertAt);

    return nullptr;
  }

  static Value *getValueAs(Value *V, PointerType *Ty,
                           Instruction *InsertAt=nullptr) {
    PointerType *ValueTy = dyn_cast<PointerType>(V->getType());
    if (!ValueTy)
      return nullptr;

    if (ValueTy == Ty)
      return V;

    BitCastInst *I = new BitCastInst(V, Ty);
    insertInstruction<IP>(I, InsertAt);

    return I;
  }

  static Value *getValueAs(Value *V, IntegerType *Ty,
                           Instruction *InsertAt=nullptr) {
    IntegerType *ValueTy = dyn_cast<IntegerType>(V->getType());
    if (!ValueTy)
      return nullptr;

    if (ValueTy == Ty)
      return V;

    if (ConstantInt *ConstV = dyn_cast<ConstantInt>(V)) {
      APInt APValue = ConstV->getValue().zextOrTrunc(Ty->getBitWidth());
      return ConstantInt::get(Ty, APValue);
    }

    ZExtInst *I = new ZExtInst(V, Ty);
    insertInstruction<IP>(I, InsertAt);

    return I;
  }
};

template<InsertPosition IP, typename T>
Value *getValueAsType(Value *V, T *Ty, Instruction *InsertAt=nullptr) {
  return GetValueAsTypeImpl<IP>::getValueAs(V, Ty, InsertAt);
}

/// Implementation class for getValueAs.
template<typename T, InsertPosition IP>
class GetValueAsImpl {
public:
  /// Implements the seec::getValueAs function.
  /// \param V the Value to convert.
  /// \param InsertAt position to insert conversion instructions.
  static Value *getValueAs(Value *V, Instruction *InsertAt) {
    return getValueAsType<IP>(V,
                              TypeBuilder<T, false>::get(V->getContext()),
                              InsertAt);
  }
};

/// Get a Value as a specific Type. If the Value does not match the type T, then
/// an instruction will be created to convert it to T, using getValueAsType.
/// \param V the Value to convert.
/// \param InsertAt position to insert conversion instructions, if they are
///                 required. The conversion instructions will be inserted
///                 relative to this position, according to IP.
template<typename T, InsertPosition IP>
Value *getValueAs(Value *V, Instruction *InsertAt=nullptr) {
  return GetValueAsImpl<T, IP>::getValueAs(V, InsertAt);
}

} // namespace seec

#endif // _SEEC_UTIL_VALUE_CONVERSION_HPP_
