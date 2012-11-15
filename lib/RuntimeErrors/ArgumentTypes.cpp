#include "seec/RuntimeErrors/ArgumentTypes.hpp"

#include "llvm/Support/ErrorHandling.h"

namespace seec {

namespace runtime_errors {

char const *describe(ArgType Type) {
  switch(Type) {
    case ArgType::None: return "None";
#define SEEC_RUNERR_ARG(TYPE) \
    case ArgType::TYPE: return #TYPE;
#include "seec/RuntimeErrors/ArgumentTypes.def"
  }
  
  llvm_unreachable("Unknown type");
  return nullptr;
}

Arg::~Arg() {}

std::unique_ptr<Arg> Arg::deserialize(uint8_t Type, uint64_t Data) {
  switch (Type) {
    case static_cast<uint8_t>(ArgType::None):
      break;
#define SEEC_RUNERR_ARG(TYPE)                 \
    case static_cast<uint8_t>(ArgType::TYPE): \
      return Arg##TYPE::deserialize(Data);
#include "seec/RuntimeErrors/ArgumentTypes.def"
  }

  llvm_unreachable("Unknown type");
  return nullptr;
}

ArgAddress::~ArgAddress() {}

ArgSelectBase::~ArgSelectBase() {}

std::unique_ptr<Arg> ArgSelectBase::deserialize(uint64_t Data) {
  uint32_t SelectType = Data >> 32; // Upper 32 bits
  uint32_t SelectValue = Data & 0xFFFFFFFF; // Lower 32 bits
  return createArgSelect(static_cast<format_selects::SelectID>(SelectType),
                         SelectValue);
}

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
  }

  llvm_unreachable("Bad SelectID given to createArgSelect");
  return nullptr;
}

ArgObject::~ArgObject() {}

ArgSize::~ArgSize() {}

ArgOperand::~ArgOperand() {}

ArgParameter::~ArgParameter() {}

} // namespace runtime_errors (in seec)

} // namespace seec
