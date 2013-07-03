//===- include/seec/clang/TypeMatch.hpp -----------------------------------===//
//
//                                    SeeC
//
// This file is distributed under The MIT License (MIT). See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
///
/// \file Support matching Clang types from different ASTContexts.
///
//===----------------------------------------------------------------------===//

#include "clang/AST/Type.h"

namespace clang {
  class ASTContext;
}

namespace seec {

namespace cm {

/// \brief Check if two types are equivalent, possibly from different contexts.
///
/// \return true if the types are equivalent.
///
bool matchImpl(::clang::ASTContext const &AContext,
               ::clang::Type const *AType,
               ::clang::ASTContext const &BContext,
               ::clang::Type const *BType);

/// \brief Check if two types are equivalent, possibly from different contexts.
///
/// This inline function checks for the trivial case where the contexts are
/// the same, and uses simple pointer comparison on the types if so. Otherwise
/// it defers to matchImpl.
///
/// \return true if the types are equivalent.
///
inline bool match(::clang::ASTContext const &AContext,
                  ::clang::Type const &AType,
                  ::clang::ASTContext const &BContext,
                  ::clang::Type const &BType)
{
  auto const ACanon = AType.getCanonicalTypeInternal().getTypePtr();
  auto const BCanon = BType.getCanonicalTypeInternal().getTypePtr();
  
  return &AContext == &BContext ? (ACanon == BCanon)
                                : matchImpl(AContext, ACanon, BContext, BCanon);
}

/// \brief Wrap a \c Type and \c ASTContext for comparison.
///
class MatchType {
  ::clang::ASTContext const *Context;
  
  ::clang::Type const *Type;
  
public:
  MatchType(::clang::ASTContext const &WithContext,
            ::clang::Type const &WithType)
  : Context(&WithContext),
    Type(&WithType)
  {}
  
  bool operator==(MatchType const &RHS) const {
    return match(*Context, *Type, *RHS.Context, *RHS.Type);
  }
  
  bool operator!=(MatchType const &RHS) const {
    return !operator==(RHS);
  }
};

} // namespace cm

} // namespace seec
