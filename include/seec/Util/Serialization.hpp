//===- include/seec/Util/Serialization.hpp -------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SEEC_UTIL_SERIALIZATION_HPP
#define SEEC_UTIL_SERIALIZATION_HPP

#include "seec/Util/Maybe.hpp"

#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace seec {

template<typename T, typename Enable = void>
struct WriteBinaryImpl;

template<typename T>
size_t writeBinary(llvm::raw_ostream &Stream, T &&Value) {
  typedef typename std::remove_reference<T>::type TRemoveRef;
  typedef typename std::remove_cv<TRemoveRef>::type BaseT;
  return WriteBinaryImpl<BaseT>::impl(Stream, std::forward<T>(Value));
}

// write integral types
template<typename T>
struct WriteBinaryImpl<T,
  typename std::enable_if<std::is_integral<T>::value>::type>
{
  static size_t impl(llvm::raw_ostream &Stream, T Value) {
    if (sizeof(T) == 1) {
      Stream.write((unsigned char)Value);
      return 1;
    }
    else {
      Stream.write((char const *)&Value, sizeof(T));
      return sizeof(T);
    }
  }
};

template<>
struct WriteBinaryImpl<std::string> {
  static size_t impl(llvm::raw_ostream &Stream, std::string const &Value) {
    size_t Size = Value.size();
    size_t Written = writeBinary(Stream, (uint64_t)Size) + Size;
    Stream.write(Value.data(), Size);
    return Written;
  }
};

template<typename T>
struct WriteBinaryImpl<std::vector<T>> {
  static size_t impl(llvm::raw_ostream &Stream, std::vector<T> const &Value) {
    size_t Written = writeBinary(Stream, (uint64_t)Value.size());

    for (auto &Element: Value) {
      Written += writeBinary(Stream, Element);
    }

    return Written;
  }
};


template<typename T, typename Enable = void>
struct GetWriteBinarySizeImpl;

template<typename T>
size_t getWriteBinarySize(T &&Value) {
  typedef typename std::remove_reference<T>::type TRemoveRef;
  typedef typename std::remove_cv<TRemoveRef>::type BaseT;
  return GetWriteBinarySizeImpl<BaseT>::impl(std::forward<T>(Value));
}

// integral types
template<typename T>
struct GetWriteBinarySizeImpl<T,
  typename std::enable_if<std::is_integral<T>::value>::type>
{
  static size_t impl(T Value) {
    return sizeof(T);
  }
};

template<>
struct GetWriteBinarySizeImpl<std::string> {
  static size_t impl(std::string const &Value) {
    return getWriteBinarySize((uint64_t)Value.size()) + Value.size();
  }
};

template<typename T>
struct GetWriteBinarySizeImpl<std::vector<T>> {
  static size_t impl(std::vector<T> const &Value) {
    size_t Size = getWriteBinarySize((uint64_t)Value.size());
    for (auto &Element: Value) {
      Size += getWriteBinarySize(Element);
    }
    return Size;
  }
};


template<typename T, typename Enable = void>
struct ReadBinaryImpl; // do not implement

template<typename T>
size_t readBinary(char const *Start, char const *End, T &Out) {
  return ReadBinaryImpl<T>::impl(Start, End, Out);
}

// read integral types
template<typename T>
struct ReadBinaryImpl<T,
  typename std::enable_if<std::is_integral<T>::value>::type>
{
  static size_t impl(char const *Start, char const *End, T &Out) {
    if (Start >= End)
      return 0;

    if (size_t(End - Start) < sizeof(T))
      return 0;

    char *OutPtr = (char *)&Out;

    for (size_t i = 0; i < sizeof(T); ++i)
      OutPtr[i] = Start[i];

    return sizeof(T);
  }
};

template<>
struct ReadBinaryImpl<std::string> {
  static size_t impl(char const *Start, char const *End, std::string &Out) {
    if (Start >= End)
      return 0;

    uint64_t Size64;
    size_t BytesRead = readBinary(Start, End, Size64);
    if (!BytesRead)
      return 0;

    assert(Size64 <= std::numeric_limits<size_t>::max());
    size_t Size = (size_t)Size64;

    Start += sizeof(uint64_t);
    if (size_t(End - Start) < Size)
      return 0;

    Out.assign(Start, Size);
    return BytesRead + Size;
  }
};

template<typename T>
struct ReadBinaryImpl<std::vector<T>> {
  static size_t impl(char const *Start, char const *End, std::vector<T> &Out) {
    if (Start >= End)
      return 0;

    // read the number of elements in the vector
    uint64_t Size64;
    size_t BytesRead = readBinary(Start, End, Size64);
    if (!BytesRead)
      return 0;
    Start += BytesRead;

    assert(Size64 <= std::numeric_limits<size_t>::max());
    size_t Size = (size_t)Size64;

    // default-initialize Size element in the vector
    Out.clear();
    Out.resize(Size);

    for (size_t i = 0; i < Size; ++i) {
      size_t Bytes = readBinary(Start, End, Out[i]);
      if (!Bytes)
        return 0;
      Start += Bytes;
      BytesRead += Bytes;
    }

    return BytesRead;
  }
};

class BinaryReader {
  char const *Start;

  char const *At;

  char const *End;

  bool Error;

public:
  BinaryReader(char const *Start, char const *End)
  : Start(Start),
    At(Start),
    End(End),
    Error(false)
  {
    assert(Start <= End);
  }

  /// \name Accessors
  /// @{

  /// Get a pointer to the start of the buffer.
  char const *start() { return Start; }

  /// Get a pointer to the current position in the buffer.
  char const *at() { return At; }

  /// Get a pointer to the end of the buffer.
  char const *end() { return End; }

  /// Determine whether or not a read error has occurred.
  bool error() { return Error; }

  /// @}

  /// \name Mutators
  /// @{

  /// Move the current position forward by Amount bytes.
  /// \param Amount the number of bytes to move forward by.
  void forward(size_t Amount) {
    At += Amount;
  }

  /// @}

  /// Read a value of type T from the buffer into Out.
  /// \tparam T the type of value to read.
  /// \param Out object that the value will be read into.
  /// \return a reference to this object.
  template<typename T>
  BinaryReader &operator>>(T &Out) {
    if (Error)
      return *this;

    size_t Read = readBinary(At, End, Out);

    if (Read) {
      forward(Read);
    }
    else {
      Error = true;
    }

    return *this;
  }
};

} // namespace seec

#endif // SEEC_UTIL_SERIALIZATION_HPP
