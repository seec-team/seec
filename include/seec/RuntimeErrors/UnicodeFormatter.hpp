#ifndef SEEC_RUNTIMEERRORS_UNICODEFORMATTER_HPP
#define SEEC_RUNTIMEERRORS_UNICODEFORMATTER_HPP

#include "unicode/unistr.h"

namespace seec {

namespace runtime_errors {
  
class RunError;

UnicodeString format(RunError const &RunErr);

} // namespace runtime_errors (in seec)

} // namespace seec

#endif // SEEC_RUNTIMEERRORS_UNICODEFORMATTER_HPP
