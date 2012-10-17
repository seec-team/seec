//===- seec/Trace/GetCurrentRuntimeValue.hpp ------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_TRACE_GETCURRENTRUNTIMEVALUE_HPP
#define SEEC_TRACE_GETCURRENTRUNTIMEVALUE_HPP

#include "seec/Trace/RuntimeValue.hpp"
#include "seec/Util/Maybe.hpp"

#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>

namespace seec {

namespace trace {

/// \brief Implementation class for getCurrentRuntimeValueAs.
///
/// This class is specialized for various types of T. If no specialization
/// exists then getCurrentRuntimeValueAs will always return an unassigned Maybe.
/// \tparam SrcTy Type of object to get raw GenericValue values from.
/// \tparam T Type to try and extract the value as.
template<typename SrcTy, typename T, typename Enable = void>
struct GetCurrentRuntimeValueAsImpl;

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
template<typename SrcTy, typename T>
struct GetCurrentRuntimeValueAsImpl<SrcTy, T *, void> {
  static seec::util::Maybe<T *>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    auto Ty = V->getType();
    
    assert(Ty->isPointerTy() && "Can't get pointer from non-pointer Value.");

    // If the Value is an Instruction, get its recorded runtime value
    if (auto I = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(I)) {
        auto IntVal = static_cast<uintptr_t>(RTValue->getUInt64());
        return reinterpret_cast<T *>(IntVal);
      }

      return seec::util::Maybe<T *>();
    }

    auto StrippedValue = V->stripPointerCasts();

    if (auto Global = llvm::dyn_cast<llvm::GlobalValue>(StrippedValue)) {
      if (auto Function = llvm::dyn_cast<llvm::Function>(Global)) {
        auto Addr = Source.getRuntimeAddress(Function);
        return Addr ? seec::util::Maybe<T *>(reinterpret_cast<T *>(Addr))
                    : seec::util::Maybe<T *>();
      }
      else if (auto GV = llvm::dyn_cast<llvm::GlobalVariable>(StrippedValue)) {
        auto Addr = Source.getRuntimeAddress(GV);
        return Addr ? seec::util::Maybe<T *>(reinterpret_cast<T *>(Addr))
                    : seec::util::Maybe<T *>();
      }
      
      llvm::errs() << "Value = " << *V << "\n";
      llvm_unreachable("Don't know how to get pointer from global value.");
    }
    else if (llvm::isa<llvm::ConstantPointerNull>(StrippedValue)) {
      return static_cast<T *>(nullptr);
    }
    
    llvm::errs() << "Value = " << *V << "\n";
    llvm_unreachable("Don't know how to get runtime value of pointer.");
    
    return seec::util::Maybe<T *>();
  }
};

/// \brief Specialization of getCurrentRuntimeValueAs to extract uint64_t types.
///
/// Because we store all pointers as uint64_t, this specialization also requires
/// the additional requirements of pointer types, namely that the following
/// member functions are present in the source type SrcTy:
///
///   uintptr_t getRuntimeAddress(Function const *);
///   uintptr_t getRuntimeAddress(GlobalVariable const *);
///
/// Which return the run-time addresses of the objects passed to them, or 0 if
/// the run-time addresses cannot be found.
///
/// \tparam SrcTy The type of object to get raw GenericValue values from.
template<typename SrcTy>
struct GetCurrentRuntimeValueAsImpl<SrcTy, uintptr_t, void> {
  static seec::util::Maybe<uintptr_t>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    auto Ty = V->getType();
    
    if (Ty->isIntegerTy()) {
      if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
        if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
          return RTValue->getUIntPtr();
        }
      }
      else if (auto ConstantInt = llvm::dyn_cast<llvm::ConstantInt>(V)) {
        return static_cast<uintptr_t>(ConstantInt->getZExtValue());
      }
    }
    else if (Ty->isPointerTy()) {
      // If the Value is an Instruction, get its recorded runtime value
      if (auto I = llvm::dyn_cast<llvm::Instruction>(V)) {
        if (auto RTValue = Source.getCurrentRuntimeValue(I)) {
          return RTValue->getUIntPtr();
        }

        return seec::util::Maybe<uintptr_t>();
      }

      // Get constant pointer values
      auto StrippedValue = V->stripPointerCasts();

      if (auto Global = llvm::dyn_cast<llvm::GlobalValue>(StrippedValue)) {
        if (auto Function = llvm::dyn_cast<llvm::Function>(Global)) {
          auto Addr = Source.getRuntimeAddress(Function);
          return Addr ? seec::util::Maybe<uintptr_t>(Addr)
                      : seec::util::Maybe<uintptr_t>();
        }
        else if (auto GV = llvm::dyn_cast<llvm::GlobalVariable>(StrippedValue)){
          auto Addr = Source.getRuntimeAddress(GV);
          return Addr ? seec::util::Maybe<uintptr_t>(Addr)
                      : seec::util::Maybe<uintptr_t>();
        }
      
        llvm::errs() << "Value = " << *V << "\n";
        llvm_unreachable("Don't know how to get pointer from global value.");
      }
      else if (llvm::isa<llvm::ConstantPointerNull>(StrippedValue)) {
        return seec::util::Maybe<uintptr_t>(static_cast<uintptr_t>(0));
      }

      llvm::errs() << "Value = " << *V << "\n";
      llvm_unreachable("Don't know how to get runtime value of pointer.");
    }
    
    return seec::util::Maybe<uintptr_t>();
  }
};

/// \brief Specialization of getCurrentRuntimeValueAs to extract signed
///        integral types.
template<typename SrcTy, typename T>
struct GetCurrentRuntimeValueAsImpl
  <SrcTy,
   T,
   typename std::enable_if<std::is_integral<T>::value
                           && std::is_signed<T>::value
                          >::type>
{
  static seec::util::Maybe<T>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    assert(V->getType()->isIntegerTy()
           && "Extracting integral type from non-integer value.");
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
        return getAs<T>(*RTValue, Instruction->getType());
      }
    }
    else if (auto ConstantInt = llvm::dyn_cast<llvm::ConstantInt>(V)) {
      // TODO: Assert that the constant isn't too large.
      return static_cast<T>(ConstantInt->getSExtValue());
    }
    
    return seec::util::Maybe<T>();
  }
};

/// \brief Specialization of getCurrentRuntimeValueAs to extract unsigned
///        integral types.
template<typename SrcTy, typename T>
struct GetCurrentRuntimeValueAsImpl
  <SrcTy,
   T,
   typename std::enable_if<std::is_integral<T>::value
                           && std::is_unsigned<T>::value
                          >::type>
{
  static seec::util::Maybe<T>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    assert(V->getType()->isIntegerTy()
           && "Extracting integral type from non-integer value.");
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
        return getAs<T>(*RTValue, Instruction->getType());
      }
    }
    else if (auto ConstantInt = llvm::dyn_cast<llvm::ConstantInt>(V)) {
      // TODO: Assert that the constant isn't too large.
      return static_cast<T>(ConstantInt->getZExtValue());
    }
    
    return seec::util::Maybe<T>();
  }
};

// Overload for float types.
template<typename SrcTy>
struct GetCurrentRuntimeValueAsImpl<SrcTy, float, void> {
  static seec::util::Maybe<float>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    assert(V->getType()->isFloatTy());
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
        return RTValue->getFloat();
      }
    }
    else if (auto ConstantFloat = llvm::dyn_cast<llvm::ConstantFP>(V)) {
      return ConstantFloat->getValueAPF().convertToFloat();
    }
    
    return seec::util::Maybe<float>();
  }
};

// Overload for double types.
template<typename SrcTy>
struct GetCurrentRuntimeValueAsImpl<SrcTy, double, void> {
  static seec::util::Maybe<double>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    assert(V->getType()->isFloatTy());
    
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
        return RTValue->getDouble();
      }
    }
    else if (auto ConstantFloat = llvm::dyn_cast<llvm::ConstantFP>(V)) {
      return ConstantFloat->getValueAPF().convertToDouble();
    }
    
    return seec::util::Maybe<double>();
  }
};

// Overload to extract the raw RuntimeValue
template<typename SrcTy>
struct GetCurrentRuntimeValueAsImpl<SrcTy, RuntimeValue const *, void> {
  static seec::util::Maybe<RuntimeValue const *>
  getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *V) {
    if (auto Instruction = llvm::dyn_cast<llvm::Instruction>(V)) {
      if (auto RTValue = Source.getCurrentRuntimeValue(Instruction)) {
        return RTValue;
      }
    }
    return seec::util::Maybe<RuntimeValue const *>();
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
seec::util::Maybe<T>
getCurrentRuntimeValueAs(SrcTy &Source, llvm::Value const *Value) {
  return GetCurrentRuntimeValueAsImpl<SrcTy, T>
          ::getCurrentRuntimeValueAs(Source, Value);
}

/// Find the values of arguments of a CallInst using getCurrentRuntimeValueAs.
// This is the base case for the variadic template below.
template<typename SrcTy>
bool getArgumentValues(SrcTy &Source,
                       llvm::CallInst const *Call,
                       size_t Offset) {
  return true;
}

/// Find the values of arguments of a CallInst using getCurrentRuntimeValueAs.
/// \tparam SrcTy The type of source object to extract run-time values from.
/// \tparam T The type to get the first argument as.
/// \tparam TS The types to get the remaining arguments as.
/// \param Source The source object to get raw RuntimeValues from.
/// \param Call The CallInst to extract the arguments of.
/// \param Offset The index into the CallInst's argument list to start
///               extracting arguments from.
/// \param Arg A reference to the object that will hold the first extracted
///            argument.
/// \param Args References to the objects that will hold the remaining extracted
///             arguments.
/// \return true if all arguments were extracted successfully.
template<typename SrcTy, typename T, typename... TS>
bool getArgumentValues(SrcTy &Source,
                       llvm::CallInst const *Call,
                       size_t Offset,
                       T& Arg,
                       TS&... Args) {  
  auto Value = getCurrentRuntimeValueAs<T>(Source, Call->getArgOperand(Offset));
  if (!Value.assigned())
    return false;
    
  Arg = Value.template get<0>();
  
  return getArgumentValues(Source, Call, Offset + 1, Args...);
}

}

}

#endif // SEEC_TRACE_GETCURRENTRUNTIMEVALUE_HPP
