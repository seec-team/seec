#include "seec/RuntimeErrors/RuntimeErrors.hpp"

namespace seec {

namespace runtime_errors {
  
char const *describe(RunErrorType T) {
  switch (T) {
#define SEEC_RUNERR(ID, ARGS) case RunErrorType::ID: return #ID;
#include "seec/RuntimeErrors/RuntimeErrors.def"
    default:
      return nullptr;
  }
}

} // namespace runtime_errors (in seec)
  
} // namespace seec
