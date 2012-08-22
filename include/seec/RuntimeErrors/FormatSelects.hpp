#ifndef SEEC_RUNTIMEERRORS_FORMATSELECTS_HPP
#define SEEC_RUNTIMEERRORS_FORMATSELECTS_HPP

#include <cstdint>

namespace seec {

namespace runtime_errors {

/// Selects used by seec::runtime_errors::ArgSelect.
namespace format_selects {

enum class SelectID : uint32_t {
#define SEEC_FORMAT_SELECT(NAME, ITEMS) NAME,
#define SEEC_FORMAT_SELECT_ITEM(NAME, ID, STR)
#include "seec/RuntimeErrors/FormatSelects.def"
};

template<typename SelectType>
struct GetSelectID;

// create select-specific enum classes and GetSelectID specializations
#define SEEC_FORMAT_SELECT(NAME, ITEMS) \
enum class NAME : uint32_t { ITEMS }; \
template<> \
struct GetSelectID<NAME> { \
  static constexpr SelectID value() { return SelectID::NAME; } \
};
#define SEEC_FORMAT_SELECT_ITEM(NAME, ID, STR) ID,
#include "seec/RuntimeErrors/FormatSelects.def"

#define SEEC_FORMAT_SELECT(NAME, ITEMS) char const *getCString(NAME Select);
#define SEEC_FORMAT_SELECT_ITEM(NAME, ID, STR)
#include "seec/RuntimeErrors/FormatSelects.def"

char const *getCString(SelectID Select, uint32_t Item);

} // namespace format_selects (in runtime_errors in seec)

} // namespace runtime_errors (in seec)

} // namespace seec

#endif // SEEC_RUNTIMEERRORS_FORMATSELECTS_HPP
