//===- Util/Endianness.hpp ------------------------------------------ C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/APInt.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/SwapByteOrder.h"
#include <cstdint>
#include <climits>

namespace seec {

namespace endianness {

typedef llvm::support::endianness Endianness;

template<Endianness To, typename T>
struct Convert {
  static T host(T Value) {
    if ((To == Endianness::little) == llvm::sys::isLittleEndianHost())
      return Value;
    return llvm::sys::SwapByteOrder(Value);
  }
};

template<Endianness To>
struct Convert<To, llvm::APInt> {
  static llvm::APInt host(llvm::APInt Value) {
    if ((To == Endianness::little) == llvm::sys::isLittleEndianHost())
      return Value;
    return Value.byteSwap();
  }
};

/// Convert a value between host-endianness and little-endianness.
template<typename T>
T little(T Value) {
  return Convert<Endianness::little, T>::host(Value);
}

/// Convert a value between host-endianness and big-endianness.
template<typename T>
T big(T Value) {
  return Convert<Endianness::big, T>::host(Value);
}

template<Endianness E, typename T>
class StoreAs {
  T Value;

public:
  StoreAs() {} // Intentionally don't initialize Value

  StoreAs(T Value)
  : Value(Convert<E, T>::host(Value))
  {}

  StoreAs<E, T> &operator=(StoreAs<E, T> const &Other) {
    Value = Other.Value;
    return *this;
  }

  StoreAs<E, T> &operator=(T Value) {
    Value = Convert<E, T>::host(Value);
    return *this;
  }

  operator T() const {
    return Convert<E, T>::host(Value);
  }
};

} // namespace endianness

} // namespace seec
