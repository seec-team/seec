//===- lib/Clang/MappedLocal.hpp ------------------------------------------===//
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

#ifndef SEEC_CLANG_MAPPEDLOCAL_HPP
#define SEEC_CLANG_MAPPEDLOCAL_HPP


#include "seec/Util/Error.hpp"
#include "seec/Util/Maybe.hpp"

namespace clang {
  class VarDecl;
}

namespace llvm {
  class MDNode;
  class Value;
}

namespace seec {

namespace seec_clang {
  class MappedModule;
}

namespace cm {

/// \brief Represents a mapping from a Clang function's local to an LLVM
///        Value.
///
class MappedLocal
{
  ::clang::VarDecl const *Declaration;
  
  ::llvm::Value const *Val;
  
public:
  /// \brief Constructor.
  ///
  MappedLocal(::clang::VarDecl const *WithDecl,
              ::llvm::Value const *WithValue)
  : Declaration{WithDecl},
    Val{WithValue}
  {}
  
  /// \brief Attempt to retrieve a MappedLocal from metadata.
  ///
  static
  seec::Maybe<MappedLocal, seec::Error>
  fromMetadata(llvm::MDNode *RootMD,
               seec_clang::MappedModule const &Module);
  
  /// \brief Get the clang::VarDecl.
  ///
  ::clang::VarDecl const *getDecl() const { return Declaration; }
  
  /// \brief Get the llvm::Value.
  ///
  ::llvm::Value const *getValue() const { return Val; }
};

} // namespace cm (in seec)

} // namespace seec


#endif // SEEC_CLANG_MAPPEDLOCAL_HPP
