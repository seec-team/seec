#ifndef SEEC_TRACE_FORMATSPECIFIERS_HPP
#define SEEC_TRACE_FORMATSPECIFIERS_HPP

#include "seec/RuntimeErrors/FormatSelects.hpp"

#include "llvm/Support/ErrorHandling.h"


namespace seec {

namespace trace {


using namespace seec::runtime_errors;


/// Length modifier that precedes a conversion specifier.
///
enum class LengthModifier {
  hh,
  h,
  none,
  l,
  ll,
  j,
  z,
  t,
  L
};

/// \brief Convert a LengthModifier to its format select representation.
///
inline
seec::runtime_errors::format_selects::CFormatLengthModifier
asCFormatLengthModifier(LengthModifier Modifier) {
  using namespace seec::runtime_errors::format_selects;
  
  switch (Modifier) {
    case LengthModifier::hh:   return CFormatLengthModifier::hh;
    case LengthModifier::h:    return CFormatLengthModifier::h;
    case LengthModifier::none: return CFormatLengthModifier::none;
    case LengthModifier::l:    return CFormatLengthModifier::l;
    case LengthModifier::ll:   return CFormatLengthModifier::ll;
    case LengthModifier::j:    return CFormatLengthModifier::j;
    case LengthModifier::z:    return CFormatLengthModifier::z;
    case LengthModifier::t:    return CFormatLengthModifier::t;
    case LengthModifier::L:    return CFormatLengthModifier::L;
  }
  
  llvm_unreachable("bad modifier.");
  return CFormatLengthModifier::none;
}


} // namespace trace (in seec)

} // namespace seec

#endif // SEEC_TRACE_FORMATSPECIFIERS_HPP
