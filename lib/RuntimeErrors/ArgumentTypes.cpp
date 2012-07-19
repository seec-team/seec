#include "seec/RuntimeErrors/ArgumentTypes.hpp"

namespace seec {

namespace runtime_errors {
  
Arg::~Arg() {}
  
ArgAddress::~ArgAddress() {}

ArgSelectBase::~ArgSelectBase() {}

std::unique_ptr<Arg> createArgSelect(format_selects::SelectID Select,
                                     uint32_t Item) {
  switch (Select) {
#define SEEC_FORMAT_SELECT(NAME, ITEMS) \
    case format_selects::SelectID::NAME: \
      return std::unique_ptr<Arg>( \
              new ArgSelect<format_selects::NAME>( \
                static_cast<format_selects::NAME>(Item)));
#define SEEC_FORMAT_SELECT_ITEM(NAME, ID, STR)
#include "seec/RuntimeErrors/FormatSelects.def"
    default:
      assert(false && "Bad SelectID given to createArgSelect");
      return nullptr;
  }
}

ArgObject::~ArgObject() {}

ArgSize::~ArgSize() {}

} // namespace runtime_errors (in seec)

} // namespace seec
