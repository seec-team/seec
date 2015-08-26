//===- lib/Util/Printing.cpp ----------------------------------------------===//
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

#include "seec/Util/Fallthrough.hpp"
#include "seec/Util/Printing.hpp"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/raw_ostream.h"

namespace seec {

namespace util {


void writeJSONStringLiteral(llvm::StringRef S, llvm::raw_ostream &Out)
{
  Out.write('"');

  auto const End = S.data() + S.size();

  for (auto It = S.data(); It != End; ++It) {
    switch (*It) {
      case '"':  Out << "\\\""; break;
      case '\\': Out << "\\\\"; break;
      case '/':  Out << "\\/";  break;
      case '\b': Out << "\\b";  break;
      case '\f': Out << "\\f";  break;
      case '\n': Out << "\\n";  break;
      case '\r': Out << "\\r";  break;
      case '\t': Out << "\\t";  break;
      default:
        Out.write(*It);
        break;
    }
  }

  Out.write('"');
}


} // namespace util

} // namespace seec
