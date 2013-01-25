//===- include/seec/RuntimeErrors/RuntimeErrors.hpp -----------------------===//
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

#ifndef SEEC_RUNTIMEERRORS_RUNTIMEERRORS_HPP
#define SEEC_RUNTIMEERRORS_RUNTIMEERRORS_HPP

#include "seec/Preprocessor/Apply.h"
#include "seec/RuntimeErrors/ArgumentTypes.hpp"
#include "seec/RuntimeErrors/FormatSelects.hpp"

#include <memory>
#include <vector>

namespace seec {

/// Classification and description of run-time errors.
namespace runtime_errors {

/// Enumeration of all known types of runtime errors.
enum class RunErrorType : uint16_t {
#define SEEC_RUNERR(ID, ARGS) \
  ID,
#include "seec/RuntimeErrors/RuntimeErrors.def"
};

/// \brief Get a string containing the textual ID of a RunErrorType.
/// \param T the RunErrorType.
/// \return a C string containing the textual ID of the value of T.
char const *describe(RunErrorType T);

/// \brief Get a string containing the name of a runtime error's argument.
/// \param Type the runtime error type.
/// \param Argument the index of the argument.
/// \return a C string containing the name of the argument.
char const *getArgumentName(RunErrorType Type, std::size_t Argument);

/// \brief An instance of a runtime error.
///
/// Represents a single occurence of a runtime error, holding the type and
/// arguments of the error.
class RunError {
  /// The type of runtime error.
  RunErrorType Type;
  
  /// The arguments used.
  std::vector<std::unique_ptr<Arg>> Args;
  
  /// Additional (subservient) runtime errors.
  std::vector<std::unique_ptr<RunError>> Additional;

public:
  /// \brief Constructor.
  /// \param Type the type of runtime error.
  /// \param Args the arguments used - will be moved from.
  RunError(RunErrorType ErrorType,
           std::vector<std::unique_ptr<Arg>> ErrorArgs,
           std::vector<std::unique_ptr<RunError>> AdditionalErrors)
  : Type(ErrorType),
    Args(std::move(ErrorArgs)),
    Additional(std::move(AdditionalErrors))
  {}
  
  
  /// \name Accessors
  /// @{
  
  /// \brief Get the type of runtime error.
  RunErrorType type() const { return Type; }
  
  /// \brief Get the arguments to this runtime error.
  decltype(Args) const &args() const { return Args; }
  
  /// \brief Get the additional errors attached to this runtime error.
  decltype(Additional) const &additional() const { return Additional; }
  
  /// @} (Accessors)
  
  
  /// \name Mutators
  /// @{
  
  /// \brief Add an additional error to this RunError.
  /// \param Error the additional error to attach to this RunError.
  /// \return this RunError.
  ///
  RunError &addAdditional(std::unique_ptr<RunError> Error) {
    Additional.emplace_back(std::move(Error));
    return *this;
  }
  
  /// @} (Mutators)
};

/// Helper function used by RunErrorCreatorBase to create an argument vector.
template<typename ContainerTy>
void emplaceArgs(ContainerTy &C) {}

/// Helper function used by RunErrorCreatorBase to create an argument vector.
template<typename ContainerTy, typename ArgType, typename... ArgTypes>
void emplaceArgs(ContainerTy &C, ArgType &&Obj, ArgTypes&&... Args) {
  C.emplace_back(new ArgType(Obj));
  emplaceArgs(C, std::forward<ArgTypes>(Args)...);
}

/// \brief Implements the create method used by RunErrorCreator.
/// \tparam Type the type of runtime error to create.
/// \tparam ArgTypes the argument types that the runtime error expects.
template<RunErrorType Type, typename... ArgTypes>
class RunErrorCreatorBase {
public:
  /// \brief Create a new runtime error of type Type with the given arguments.
  /// \param Args the arguments used for the new runtime error.
  /// \return a unique_ptr owning the new runtime error.
  static std::unique_ptr<RunError> create(ArgTypes&&... Args) {
    std::vector<std::unique_ptr<Arg>> ArgsVec;
    emplaceArgs(ArgsVec, std::forward<ArgTypes>(Args)...);
    
    auto Error = new RunError(Type,
                              std::move(ArgsVec),
                              std::vector<std::unique_ptr<RunError>>{});
    
    return std::unique_ptr<RunError>(Error);
  }
};

/// \brief Class used by createRunError to construct runtime errors.
template<RunErrorType Type>
class RunErrorCreator {};

#define SEEC_RUNERR_TYPE(NAME, TYPE) TYPE
#define SEEC_RUNERR(ID, ARGS)                                                  \
template<>                                                                     \
class RunErrorCreator<RunErrorType::ID>                                        \
: public RunErrorCreatorBase<RunErrorType::ID,                                 \
                             SEEC_PP_APPLY_WITH_SEP(                           \
                               SEEC_RUNERR_TYPE,                               \
                               SEEC_PP_APPLY_COMMA_SEPARATOR,                  \
                               ARGS)>                                          \
{};
#include "seec/RuntimeErrors/RuntimeErrors.def"
#undef SEEC_RUNERR_TYPE

/// \brief Construct a new runtime error.
///
/// This function simply forwards its arguments to the static create function
/// provided by RunErrorCreator, which is specialized by the type of runtime
/// error being created. The type of argument should either be the argument
/// expected by the error, or a type that can be used to implicitly construct
/// the argument expected by the error.
///
/// \tparam Type the type of runtime error.
/// \tparam ArgTypes the types of arguments used to construct the error.
/// \param Args the arguments used to construct the error.
/// \return a unique_ptr owning the new runtime error.
template<RunErrorType Type, typename... ArgTypes>
std::unique_ptr<RunError> createRunError(ArgTypes&&... Args) {
  return RunErrorCreator<Type>::create(std::forward<ArgTypes>(Args)...);
}

} // namespace runtime_errors (in seec)
  
} // namespace seec

#endif // SEEC_RUNTIMEERRORS_RUNTIMEERRORS_HPP
