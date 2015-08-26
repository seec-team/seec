//===- lib/RuntimeErrors/RuntimeErrors.cpp --------------------------------===//
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

#include "seec/RuntimeErrors/RuntimeErrors.hpp"

namespace seec {

namespace runtime_errors {
  
char const *describe(RunErrorType T) {
  switch (T) {
#define SEEC_RUNERR(ID, ARGS) \
    case RunErrorType::ID: return #ID;
#include "seec/RuntimeErrors/RuntimeErrors.def"
    default:
      return nullptr;
  }
}

char const *getArgumentName(RunErrorType Type, std::size_t Argument) {
  switch (Type) {
    // Generate the switch table from the RuntimeErrors X-Macro.
#define SEEC_RUNERR_STRINGIZE_NAME(NAME, TYPE) #NAME
#define SEEC_RUNERR(ID, ARGS)                                                  \
    case RunErrorType::ID:                                                     \
    {                                                                          \
      char const *Names[] = {                                                  \
        SEEC_PP_APPLY_WITH_SEP(SEEC_RUNERR_STRINGIZE_NAME,                     \
                               SEEC_PP_APPLY_COMMA_SEPARATOR,                  \
                               ARGS)                                           \
      };                                                                       \
      assert(Argument < (sizeof(Names) / sizeof(char const *)));               \
      return Names[Argument];                                                  \
    }
#include "seec/RuntimeErrors/RuntimeErrors.def"
#undef SEEC_RUNERR_STRINGIZE_NAME

  }
  
  return nullptr;
}

std::unique_ptr<RunError> RunError::clone() const
{
  std::vector<std::unique_ptr<Arg>> CloneArgs;
  for (auto const &Arg : Args)
    CloneArgs.emplace_back(Arg->clone());

  std::vector<std::unique_ptr<RunError>> CloneAdditional;
  for (auto const &Err : Additional)
    CloneAdditional.emplace_back(Err->clone());

  return std::unique_ptr<RunError>(new RunError(Type,
                                                std::move(CloneArgs),
                                                std::move(CloneAdditional)));
}

} // namespace runtime_errors (in seec)
  
} // namespace seec
