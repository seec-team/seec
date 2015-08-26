//===- seec/Trace/GetCurrentRuntimeValue.hpp ------------------------ C++ -===//
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

#ifndef SEEC_TRACE_GETCURRENTRUNTIMEVALUE_HPP
#define SEEC_TRACE_GETCURRENTRUNTIMEVALUE_HPP

#include "seec/DSA/MemoryArea.hpp"
#include "seec/Trace/RuntimeValue.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <cfloat>

namespace seec {

namespace trace {

/// \brief Implementation class for getCurrentRuntimeValueAs.
///
/// This class is specialized for various types of T. If no specialization
/// exists then getCurrentRuntimeValueAs will always return an unassigned Maybe.
/// \tparam T Type to try and extract the value as.
template<typename T, typename Enable = void>
struct GetCurrentRuntimeValueAsImpl;

/// \brief Specialization of getCurrentRuntimeValueAs to extract uintptr_t.
///
/// Because we store all pointers as uintptr_t, this specialization also
/// requires the additional requirements of pointer types, namely that the
/// following member functions are present in the source type SrcTy:
///
///   uintptr_t getRuntimeAddress(Function const *);
///   uintptr_t getRuntimeAddress(GlobalVariable const *);
///   llvm::DataLayout const &getDataLayout();
///
/// Which return the run-time addresses of the objects passed to them, or 0 if
/// the run-time addresses cannot be found.
///
/// To retrieve the value of byval Argument pointers, the following member
/// function must also be present in the source type SrcTy:
///
///   seec::Maybe<seec::MemoryArea>
///   getParamByValArea(llvm::Argument const *Arg) const;
///
/// This function returns the area occupied by the byval parameter. The start
/// of this area will be used as the value of the Argument.
///
/// \tparam SrcTy The type of object to get raw GenericValue values from.
template<>
struct GetCurrentRuntimeValueAsImpl<uintptr_t, void> {
  template<typename SrcTy>
  static seec::Maybe<uintptr_t>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    auto Ty = V->getType();
    
    if (Ty->isIntegerTy()) {
      if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
        if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
          return RTValue->getUIntPtr();
        }
        
        return seec::Maybe<uintptr_t>();
      }
      else if (auto ConstantInt = llvm::dyn_cast<llvm::ConstantInt>(V)) {
        return static_cast<uintptr_t>(ConstantInt->getZExtValue());
      }
      
      llvm_unreachable("Don't know how to extract integer.");
    }
    else if (Ty->isPointerTy()) {
      // If the Value is an Instruction, get its recorded runtime value
      if (auto I = llvm::dyn_cast<llvm::Instruction>(V)) {
        if (auto RTValue = Source.getCurrentRuntimeValue(I)) {
          return RTValue->getUIntPtr();
        }

        return seec::Maybe<uintptr_t>();
      }

      // Get constant pointer values
      auto StrippedValue = V->stripPointerCasts();

      if (auto Global = llvm::dyn_cast<llvm::GlobalValue>(StrippedValue)) {
        if (auto Function = llvm::dyn_cast<llvm::Function>(Global)) {
          auto Addr = Source.getRuntimeAddress(Function);
          return Addr ? seec::Maybe<uintptr_t>(Addr)
                      : seec::Maybe<uintptr_t>();
        }
        else if (auto GV = llvm::dyn_cast<llvm::GlobalVariable>(StrippedValue)){
          auto Addr = Source.getRuntimeAddress(GV);
          return Addr ? seec::Maybe<uintptr_t>(Addr)
                      : seec::Maybe<uintptr_t>();
        }
      
        llvm::errs() << "Value = " << *V << "\n";
        llvm_unreachable("Don't know how to get pointer from global value.");
      }
      else if (llvm::isa<llvm::ConstantPointerNull>(StrippedValue)) {
        return seec::Maybe<uintptr_t>(static_cast<uintptr_t>(0));
      }
      
      // Get byval argument values
      if (auto const Arg = llvm::dyn_cast<llvm::Argument>(V)) {
        if (Arg->hasByValAttr()) {
          auto const MaybeArea = Source.getParamByValArea(Arg);
          if (MaybeArea.template assigned<seec::MemoryArea>()) {
            auto const Val = MaybeArea.template get<seec::MemoryArea>().start();
            return static_cast<uintptr_t>(Val);
          }
          return seec::Maybe<uintptr_t>();
        }
        
        llvm::errs() << "Value = " << *V << "\n";
        llvm_unreachable("Don't know how to get pointer from argument.");
      }

      // Handle some ConstantExpr operations. A better way to do this, if we
      // can, would be to replace the used values that are runtime constants
      // (e.g. global variable addresses) with simple constants and get LLVM
      // to deduce the value.
      if (auto const CE = llvm::dyn_cast<llvm::ConstantExpr>(V)) {
        switch (CE->getOpcode()) {
          case llvm::Instruction::MemoryOps::GetElementPtr:
          {
            llvm::DataLayout const &DL = Source.getDataLayout();
            auto const Base = CE->getOperand(0);

            auto ElemAddress = getCurrentRuntimeValueAs(Source, Base)
                               .template get<uintptr_t>();
            llvm::Type *ElemType = Base->getType();

            auto const NumOperands = CE->getNumOperands();

            for (unsigned i = 1; i < NumOperands; ++i) {
              auto const MaybeValue =
                getCurrentRuntimeValueAs(Source, CE->getOperand(i));

              if (!MaybeValue.template assigned<uintptr_t>()) {
                llvm::errs() << "GetElementPtr const expr couldn't value of: "
                            << *(CE->getOperand(i)) << "\n";
                return Maybe<uintptr_t>();
              }

              auto const Value = MaybeValue.template get<uintptr_t>();

              if (auto ST = llvm::dyn_cast<llvm::SequentialType>(ElemType)) {
                ElemType = ST->getElementType();
                ElemAddress += (Value * DL.getTypeAllocSize(ElemType));
              }
              else if (auto ST = llvm::dyn_cast<llvm::StructType>(ElemType)) {
                auto const Layout = DL.getStructLayout(ST);
                ElemType = ST->getElementType(Value);
                ElemAddress += Layout->getElementOffset(Value);
              }
            }

            return ElemAddress;
          }

          default:
            llvm::errs() << "can't get runtime value of const expr pointer: "
                         << *CE << "\n";
            return Maybe<uintptr_t>();
        }
      }

      llvm::errs() << "don't know how to get runtime value of pointer: "
                   << *V << "\n";
    }

    return Maybe<uintptr_t>();
  }
};

/// \brief Specialization of getCurrentRuntimeValueAs to extract pointer types.
///
/// Pointer types additionally require that the following member functions are
/// present in the source type SrcTy:
///
///   uintptr_t getRuntimeAddress(Function const *);
///   uintptr_t getRuntimeAddress(GlobalVariable const *);
///
/// Which return the run-time addresses of the objects passed to them, or 0 if
/// the run-time addresses cannot be found.
///
/// \tparam SrcTy The type of object to get raw GenericValue values from.
/// \tparam T The base type to try and extract the value as a pointer to.
template<typename T>
struct GetCurrentRuntimeValueAsImpl<T *, void> {
  template<typename SrcTy>
  static seec::Maybe<T *>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    auto Addr = GetCurrentRuntimeValueAsImpl<uintptr_t>::
                  getCurrentRuntimeValueAs(Source, V);
    
    if (Addr.assigned())
      return reinterpret_cast<T *>(Addr.template get<0>());
    
    return seec::Maybe<T *>();
  }
};

/// \brief Specialization of getCurrentRuntimeValueAs to extract signed
///        integral types.
template<typename T>
struct GetCurrentRuntimeValueAsImpl
  <T,
   typename std::enable_if<std::is_integral<T>::value
                           && std::is_signed<T>::value
                          >::type>
{
  template<typename SrcTy>
  static seec::Maybe<T>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    if (!V->getType()->isIntegerTy())
      return seec::Maybe<T>();
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction))
        return getAs<T>(*RTValue, Instruction->getType());
      return seec::Maybe<T>();
    }
    else if (auto ConstantInt = llvm::dyn_cast<llvm::ConstantInt>(V)) {
      // TODO: Assert that the constant isn't too large.
      return static_cast<T>(ConstantInt->getSExtValue());
    }
    
    llvm_unreachable("Don't know how to extract signed integral type.");
    return seec::Maybe<T>();
  }
};

/// \brief Specialization of getCurrentRuntimeValueAs to extract unsigned
///        integral types.
template<typename T>
struct GetCurrentRuntimeValueAsImpl
  <T,
   typename std::enable_if<std::is_integral<T>::value
                           && std::is_unsigned<T>::value
                          >::type>
{
  template<typename SrcTy>
  static seec::Maybe<T>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    if (!V->getType()->isIntegerTy())
      return seec::Maybe<T>();
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction))
        return getAs<T>(*RTValue, Instruction->getType());
      return seec::Maybe<T>();
    }
    else if (auto ConstantInt = llvm::dyn_cast<llvm::ConstantInt>(V)) {
      // TODO: Assert that the constant isn't too large.
      return static_cast<T>(ConstantInt->getZExtValue());
    }
    
    llvm_unreachable("Don't know how to extract unsigned integral type.");
    return seec::Maybe<T>();
  }
};

// Overload for float types.
template<>
struct GetCurrentRuntimeValueAsImpl<float, void> {
  template<typename SrcTy>
  static seec::Maybe<float>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    if (!V->getType()->isFloatTy())
      return seec::Maybe<float>();
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction))
        return RTValue->getFloat();
      return seec::Maybe<float>();
    }
    else if (auto ConstantFloat = llvm::dyn_cast<llvm::ConstantFP>(V)) {
      return ConstantFloat->getValueAPF().convertToFloat();
    }
    
    llvm_unreachable("Don't know how to extract float!");
    return seec::Maybe<float>();
  }
};

// Overload for double types.
template<>
struct GetCurrentRuntimeValueAsImpl<double, void> {
  template<typename SrcTy>
  static seec::Maybe<double>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    if (!V->getType()->isDoubleTy())
      return seec::Maybe<double>();
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction))
        return RTValue->getDouble();
      return seec::Maybe<double>();
    }
    else if (auto ConstantFloat = llvm::dyn_cast<llvm::ConstantFP>(V)) {
      return ConstantFloat->getValueAPF().convertToDouble();
    }
    
    llvm_unreachable("Don't know how to extract double!");
    return seec::Maybe<double>();
  }
};

// Overload for long double types.
template<>
struct GetCurrentRuntimeValueAsImpl<long double, void> {
  template<typename SrcTy>
  static seec::Maybe<long double>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    // Ensure that V's type matches one of the "long double" types.
    auto const VTy = V->getType();
    if (!VTy->isX86_FP80Ty() && !VTy->isPPC_FP128Ty())
      return seec::Maybe<long double>();
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction))
        return static_cast<long double>(RTValue->getLongDouble());
      return seec::Maybe<long double>();
    }
    else if (auto ConstantFloat = llvm::dyn_cast<llvm::ConstantFP>(V)) {
      auto const APF = ConstantFloat->getValueAPF();
      auto const SemanticsPtr = &(APF.getSemantics());

      if (SemanticsPtr == &llvm::APFloat::IEEEsingle) {
        return static_cast<long double>(APF.convertToFloat());
      }
      else if (SemanticsPtr == &llvm::APFloat::IEEEdouble) {
        return static_cast<long double>(APF.convertToDouble());
      }
      else if (SemanticsPtr == &llvm::APFloat::x87DoubleExtended) {
        assert(LDBL_MANT_DIG == 64);
        long double Result = 0;
        auto const API = APF.bitcastToAPInt();
        memcpy(reinterpret_cast<char *>(&Result),
               reinterpret_cast<char const *>(API.getRawData()),
               10);
        return Result;
      }
      else {
        llvm_unreachable("Float semantics not yet supported!");
      }
    }
    
    llvm_unreachable("Don't know how to extract long double!");
    return seec::Maybe<long double>();
  }
};

// Overload to extract the raw RuntimeValue
template<>
struct GetCurrentRuntimeValueAsImpl<RuntimeValue const *, void> {
  template<typename SrcTy>
  static seec::Maybe<RuntimeValue const *>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction))
        return RTValue;
      return seec::Maybe<RuntimeValue const *>();
    }
    
    llvm_unreachable("Don't know how to extract RuntimeValue.");
    return seec::Maybe<RuntimeValue const *>();
  }
};

/// \brief Find the current value of a Value.
///
/// This requires the source object to implement the function:
///
///   RuntimeValue const *getCurrentRuntimeValue(llvm::Instruction const *I)
///
/// \tparam T The type to extract the value as.
/// \tparam SrcTy The type of source object to get raw RuntimeValues from.
/// \param Source The source object to get raw RuntimeValues from.
/// \param V The Value that we will try to find the run-time value for.
template<typename T, typename SrcTy>
seec::Maybe<T>
getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *Value) {
  return GetCurrentRuntimeValueAsImpl<T>
          ::getCurrentRuntimeValueAs(Source, Value);
}

}

}

#endif // SEEC_TRACE_GETCURRENTRUNTIMEVALUE_HPP
