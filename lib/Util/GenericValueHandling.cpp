//===- Util/GenericValueHandling.cpp -------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "seec/Util/Endianness.hpp"
#include "seec/Util/GenericValueHandling.hpp"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstring>
#include <new>

using namespace llvm;
using namespace seec::endianness;

namespace seec {

void printGenericValueAsType(raw_ostream &Out,
                             GenericValue const * GV,
                             Type const * Ty) {
  switch(Ty->getTypeID()) {
    case Type::TypeID::VoidTyID:
      return;

    case Type::TypeID::FloatTyID:
      Out << GV->FloatVal;
      return;

    case Type::TypeID::DoubleTyID:
      Out << GV->DoubleVal;
      return;

    case Type::TypeID::PointerTyID:
      Out << GV->PointerVal;
      return;

    case Type::TypeID::IntegerTyID:
      GV->IntVal.print(Out, false);
      return;

    default:
      break;
  }

#if 0 // TODO: implement
  else if (StructType const *StructTy = dyn_cast<StructType>(Ty)) {

  }
  else if (ArrayType const *ArrayTy = dyn_cast<ArrayType>(Ty)) {

  }
  else if (VectorType const *VectorTy = dyn_cast<VectorType>(Ty)) {

  }
#endif
}

void writeGenericValueAsType(raw_fd_ostream &Out,
                             GenericValue const *GV,
                             Type const *Ty) {
  switch(Ty->getTypeID()) {
    case Type::TypeID::VoidTyID:
      return;

    case Type::TypeID::FloatTyID:
      Out.write((char const *)(&GV->FloatVal), sizeof(GV->FloatVal));
      return;

    case Type::TypeID::DoubleTyID:
      Out.write((char const *)(&GV->DoubleVal), sizeof(GV->DoubleVal));
      return;

    case Type::TypeID::PointerTyID:
    {
      auto Val = little(uint64_t(GV->PointerVal));
      Out.write((char const *)&Val, sizeof(Val));
      return;
    }

    case Type::TypeID::IntegerTyID:
    {
      APInt Val = little(GV->IntVal);
      auto Data = Val.getRawData();
      Out.write((char const *)Data, sizeof(uint64_t) * Val.getNumWords());
      return;
    }

    default:
      break;
  }

  // TODO: Struct, Array, Vector
}

char const *readGenericValueOfType(char const *Start,
                                   char const *End,
                                   Type const *Ty,
                                   GenericValue &Out) {
  assert(Start <= End && Start && End && Ty && "Preconditions violated.");

  switch(Ty->getTypeID()) {
    case Type::TypeID::VoidTyID:
      return Start;

    case Type::TypeID::FloatTyID:
    {
      if (sizeof(Out.FloatVal) > size_t(End - Start))
        return nullptr;

      memcpy((char *)&Out.FloatVal, Start, sizeof(Out.FloatVal));
      return Start + sizeof(Out.FloatVal);
    }

    case Type::TypeID::DoubleTyID:
    {
      if (sizeof(Out.DoubleVal) > size_t(End - Start))
        return nullptr;

      memcpy((char *)&Out.DoubleVal, Start, sizeof(Out.DoubleVal));
      return Start + sizeof(Out.DoubleVal);
    }

    case Type::TypeID::PointerTyID:
    {
      if (sizeof(uint64_t) > size_t(End - Start))
        return nullptr;

      uint64_t Val {0};
      memcpy((char *)&Val, Start, sizeof(uint64_t));
      Out.PointerVal = (void *)Val;
      return Start + sizeof(uint64_t);
    }

    case Type::TypeID::IntegerTyID:
    {
      unsigned BitWidth = cast<IntegerType>(Ty)->getBitWidth();
      unsigned BitsPerWord = CHAR_BIT * sizeof(uint64_t);
      unsigned Words = BitWidth / BitsPerWord;

      if (Words * BitsPerWord < BitWidth)
        ++Words;

      unsigned Bytes = Words * sizeof(uint64_t);

      if (Bytes > size_t(End - Start))
        return nullptr;

      std::unique_ptr<uint64_t[]> Data (new (std::nothrow) uint64_t[Words]);
      if (!Data)
        return nullptr;

      memcpy((char *)Data.get(), Start, Bytes);
      Out.IntVal = APInt(BitWidth, Words, Data.get());

      return Start + Bytes;
    }

    default:
      break;
  }

  // TODO: Struct, Array, Vector

  return nullptr;
}

} // namespace seec
