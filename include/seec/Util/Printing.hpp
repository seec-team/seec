//===- seec/Util/Printing.hpp --------------------------------------- C++ -===//
//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _SEEC_UTIL_PRINTING_HPP_
#define _SEEC_UTIL_PRINTING_HPP_

#include "llvm/Support/DataTypes.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

class raw_ostream;

}

namespace seec {

namespace util {

inline
void write_hex_byte(llvm::raw_ostream &Out, unsigned char Byte) {
  unsigned char High = Byte >> 4;
  unsigned char Low = Byte & 0xF;

  Out.write((High < 10) ? ('0' + High) : ('a' + (High - 10)));
  Out.write((Low < 10) ? ('0' + Low) : ('a' + (Low - 10)));
}

inline
void write_hex_bytes(llvm::raw_ostream &Out, unsigned char const *Start,
                     size_t Length) {
  for (auto End = Start + Length; Start != End; ++Start) {
    write_hex_byte(Out, *Start);
  }
}

inline
void write_hex_bytes(llvm::raw_ostream &Out, signed char const *Start,
                     size_t Length) {
  write_hex_bytes(Out, reinterpret_cast<unsigned char const *>(Start), Length);
}

inline
void write_hex_bytes(llvm::raw_ostream &Out, char const *Start,
                     size_t Length) {
  write_hex_bytes(Out, reinterpret_cast<unsigned char const *>(Start), Length);
}

template<typename T>
void write_hex_padded(llvm::raw_ostream &Out, T Value) {
  Out << "0x";

  auto Shift = 8 * (sizeof(T) - 1);

  for (unsigned i = 0; i < sizeof(T); ++i) {
    write_hex_byte(Out, (Value >> Shift) & 0xFF);
    Shift -= 8;
  }
}

} // namespace util

} // namespace seec

#endif // _SEEC_UTIL_PRINTING_HPP_
