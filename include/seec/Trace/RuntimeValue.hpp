//===- seec/Trace/RuntimeValue.hpp ---------------------------------- C++ -===//
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

#ifndef SEEC_TRACE_RUNTIMEVALUE_HPP
#define SEEC_TRACE_RUNTIMEVALUE_HPP

#include "seec/Trace/TraceFormat.hpp"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/Support/ErrorHandling.h"

#include <cstdint>
#include <cfloat>
#include <limits>

namespace seec {

namespace trace {

class RuntimeValue {
  offset_uint RecordOffset;

  RuntimeValueRecord Data;

public:
  RuntimeValue()
  : RecordOffset(seec::trace::noOffset()),
    Data()
  {}

  offset_uint getRecordOffset() const { return RecordOffset; }

  bool assigned() const { return RecordOffset != seec::trace::noOffset(); }
  
  void set(offset_uint Offset, RuntimeValueRecord Value) {
    RecordOffset = Offset;
    Data = Value;
  }
  
  void clear() { RecordOffset = seec::trace::noOffset(); }

  // set for integral values
  template<typename T>
  void set(offset_uint Offset,
           T&& Value,
           // dummy parameter to enable only for integers
            typename std::enable_if<
              std::is_integral<
                typename std::remove_reference<T>::type
              >::value
            >::type *Dummy = nullptr
           ) {
    RecordOffset = Offset;
    Data.UInt64 = static_cast<uint64_t>(Value);
  }
  
  void set(offset_uint Offset, uintptr_t Value) {
    RecordOffset = Offset;
    Data.UIntPtr = Value;
  }

  void set(offset_uint Offset, float Value) {
    RecordOffset = Offset;
    Data.Float = Value;
  }

  void set(offset_uint Offset, double Value) {
    RecordOffset = Offset;
    Data.Double = Value;
  }
  
  void set(offset_uint Offset, long double Value) {
    RecordOffset = Offset;
    Data.LongDouble = Value;
  }

  llvm::APFloat getAPFloat(llvm::Type *Type) const {
    switch (Type->getTypeID()) {
      case llvm::Type::FloatTyID:    return llvm::APFloat(Data.Float);
      case llvm::Type::DoubleTyID:   return llvm::APFloat(Data.Double);
      case llvm::Type::X86_FP80TyID:
      {
        assert(LDBL_MANT_DIG == 64);
        uint64_t Vals[2] = {0, 0}; // 16 bytes.
        memcpy(reinterpret_cast<char *>(Vals),
               reinterpret_cast<char const *>(&(Data.LongDouble)),
               10);
        return llvm::APFloat(llvm::APFloat::x87DoubleExtended,
                             llvm::APInt(80, Vals));
      }
      case llvm::Type::HalfTyID:      break; // TODO
      case llvm::Type::FP128TyID:     break; // TODO
      case llvm::Type::PPC_FP128TyID: break; // TODO
      case llvm::Type::VoidTyID:      break;
      case llvm::Type::LabelTyID:     break;
      case llvm::Type::MetadataTyID:  break;
      case llvm::Type::X86_MMXTyID:   break;
      case llvm::Type::IntegerTyID:   break;
      case llvm::Type::FunctionTyID:  break;
      case llvm::Type::StructTyID:    break;
      case llvm::Type::ArrayTyID:     break;
      case llvm::Type::PointerTyID:   break;
      case llvm::Type::VectorTyID:    break;
      case llvm::Type::NumTypeIDs:    break;
    }

    llvm_unreachable("getAPFloat called with unsupported type.");
    return llvm::APFloat(0.0);
  }
  
  llvm::APInt getAPInt(llvm::IntegerType *Type) const {
    auto BitWidth = Type->getBitWidth();
    assert(BitWidth <= 64 && "Can't get int with more than 64 bits.");
    return llvm::APInt(BitWidth, Data.UInt64);
  }

  llvm::APSInt getAPSIntSigned(llvm::IntegerType *Type) const {
    return llvm::APSInt(getAPInt(Type), /* isUnsigned*/ false);
  }
  
  decltype(Data) const &getData() const { return Data; }
  
  uint64_t getUInt64() const { return Data.UInt64; }
  
  uintptr_t getUIntPtr() const { return Data.UIntPtr; }

  float getFloat() const { return Data.Float; }

  double getDouble() const { return Data.Double; }
  
  long double getLongDouble() const { return Data.LongDouble; }
};

template<typename T, typename Enable = void>
struct GetAsImpl; // do not implement

// Specialization to get signed integer types.
template<typename T>
struct GetAsImpl<T, 
                 typename std::enable_if<std::is_integral<T>::value
                                         && std::is_signed<T>::value>::type
                > {
  static T impl(RuntimeValue const &Value, llvm::Type const *Type) {
    auto IntType = llvm::dyn_cast<llvm::IntegerType>(Type);
    assert(IntType && "Extracting int from non-integer type?");
    
    uint64_t V = Value.getUInt64();
    
    if (V & IntType->getSignBit()) {
      // fill the unused bits with 1s (sign-extend twos-complement)
      V |= ~IntType->getBitMask();
    }
    
    return static_cast<T>(V);
  }
};

// Specialization to get unsigned integer types.
template<typename T>
struct GetAsImpl<T,
                 typename std::enable_if<std::is_integral<T>::value
                                         && std::is_unsigned<T>::value>::type
                > {
  static T impl(RuntimeValue const &Value, llvm::Type const *Type) {
    assert(Type->isIntegerTy());
    return static_cast<T>(Value.getUInt64());
  }
};

template<>
struct GetAsImpl<float> {
  static float impl(RuntimeValue const &Value, llvm::Type const *Type) {
    assert(Type->isFloatTy());
    return Value.getFloat();
  }
};

template<>
struct GetAsImpl<double> {
  static double impl(RuntimeValue const &Value, llvm::Type const *Type) {
    assert(Type->isDoubleTy());
    return Value.getDouble();
  }
};

template<>
struct GetAsImpl<long double> {
  static long double impl(RuntimeValue const &Value, llvm::Type const *Type) {
    assert(Type->isX86_FP80Ty() || Type->isFP128Ty() || Type->isPPC_FP128Ty());
    return Value.getLongDouble();
  }
};

template<typename T>
struct GetAsImpl<T *> {
  static T *impl(RuntimeValue const &Value, llvm::Type const *Type) {
    assert(Type->isPointerTy());
    return reinterpret_cast<T *>(Value.getUIntPtr());
  }
};

template<typename T>
T getAs(RuntimeValue const &Value, llvm::Type const *Type) {
  return GetAsImpl<T>::impl(Value, Type);
}

} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_RUNTIMEVALUE_HPP
