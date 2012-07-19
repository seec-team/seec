#include "seec/RuntimeErrors/FormatSelects.hpp"

namespace seec {

namespace runtime_errors {
  
namespace format_selects {

#define SEEC_FORMAT_SELECT(NAME, ITEMS) \
char const *getCString(NAME Select) { \
  switch (Select) { \
    ITEMS \
    default: return nullptr; \
  } \
}
#define SEEC_FORMAT_SELECT_ITEM(NAME, ID, STR) case NAME::ID: return STR;
#include "seec/RuntimeErrors/FormatSelects.def"

char const *getCString(SelectID Select, uint32_t Item) {
  switch (Select) {
#define SEEC_FORMAT_SELECT(NAME, ITEMS) \
    case SelectID::NAME: \
      return getCString(NAME(Item));
#define SEEC_FORMAT_SELECT_ITEM(NAME, ID, STR)
#include "seec/RuntimeErrors/FormatSelects.def"
    default:
      return nullptr;
  }
}

} // namespace format_selects (in runtime_errors in seec)

} // namespace runtime_errors (in seec)
  
} // namespace seec
