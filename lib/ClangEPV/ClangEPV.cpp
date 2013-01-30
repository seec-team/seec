//===- lib/ClangEPV/ClangEPV.cpp ------------------------------------------===//
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


#include "seec/ClangEPV/ClangEPV.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclFriend.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/StmtObjC.h"

#include "llvm/Support/raw_ostream.h"

#include "unicode/fmtable.h"
#include "unicode/msgfmt.h"

#include <vector>


namespace seec {

namespace clang_epv {


//===----------------------------------------------------------------------===//
// Formattable helpers.
//===----------------------------------------------------------------------===//

template<typename T>
Formattable formatAsBool(T &&Value) {
  return Value ? Formattable("true") : Formattable("false");
}


//===----------------------------------------------------------------------===//
// Explanation
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// explain()
//===----------------------------------------------------------------------===//

seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Decl const *Node)
{
  if (Node == nullptr)
    return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                {"errors",
                                                 "ExplainNullDecl"}));
  
  return ExplanationOfDecl::create(Node);
}


seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Stmt const *Node)
{
  if (Node == nullptr)
    return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                {"errors",
                                                 "ExplainNullDecl"}));
  
  return ExplanationOfStmt::create(Node);
}


//===----------------------------------------------------------------------===//
// ExplanationOfDecl
//===----------------------------------------------------------------------===//

/// \brief Catch all non-specialized Decl cases.
///
void addInfo(::clang::Decl const *Decl,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{}

seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
ExplanationOfDecl::create(::clang::Decl const *Node)
{
  char const *DescriptionKey = nullptr;
  seec::icu::FormatArgumentsWithNames DescriptionArguments;
  
  NodeLinks ExplanationLinks;
  
  // Find the appropriate description for the Decl kind.
  switch (Node->getKind()) {
#define DECL(DERIVED, BASE)                       \
    case ::clang::Decl::Kind::DERIVED:            \
      DescriptionKey = #DERIVED;                  \
      addInfo(llvm::cast<::clang::DERIVED##Decl>(Node), \
              DescriptionArguments,               \
              ExplanationLinks);                  \
      break;
#define ABSTRACT_DECL(DECL)
#include "clang/AST/DeclNodes.inc"
    
    default:
      return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                  {"errors",
                                                   "CreateDeclUnknownDeclKind"
                                                   }));
  }
  
  // Extract the raw description.
  UErrorCode Status = U_ZERO_ERROR;
  auto const Descriptions = seec::getResource("ClangEPV",
                                              Locale(),
                                              Status,
                                              "descriptions");
  auto const Description = Descriptions.getStringEx(DescriptionKey, Status);
  
  if (!U_SUCCESS(Status))
    return seec::Error(
              LazyMessageByRef::create("ClangEPV",
                                       {"errors", "DescriptionNotFound"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Format the raw description.
  UnicodeString FormattedDescription = seec::icu::format(Description,
                                                         DescriptionArguments,
                                                         Status);
  if (!U_SUCCESS(Status))
    return seec::Error(
              LazyMessageByRef::create("ClangEPV",
                                       {"errors", "DescriptionFormatFailed"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Get the indexed description.
  auto MaybeIndexed = seec::icu::IndexedString::from(FormattedDescription);
  if (!MaybeIndexed.assigned())
    return seec::Error(
              LazyMessageByRef::create("ClangEPV",
                                       {"errors", "DescriptionIndexFailed"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Produce the Explanation.
  auto Explained = new ExplanationOfDecl(Node,
                                         std::move(MaybeIndexed.get<0>()),
                                         std::move(ExplanationLinks));
  
  return std::unique_ptr<Explanation>(Explained);
}


//===----------------------------------------------------------------------===//
// ExplanationOfStmt
//===----------------------------------------------------------------------===//

/// \brief Catch all non-specialized Stmt cases.
///
void addInfo(::clang::Stmt const *Statement,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{}

/// \brief Specialization for IfStmt.
///
void addInfo(::clang::IfStmt const *Statement,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  Arguments.add("has_condition_variable",
                formatAsBool(Statement->getConditionVariable()));
  Arguments.add("has_else",
                formatAsBool(Statement->getElse()));
  
  Links.add("cond", Statement->getCond());
  Links.add("then", Statement->getThen());
  Links.add("else", Statement->getElse());
}

/// \brief Specialization for ReturnStmt.
///
void addInfo(::clang::ReturnStmt const *Statement,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  Arguments.add("has_return_value",
                formatAsBool(Statement->getRetValue()));
}

/// \brief Attempt to create an Explanation for a ::clang::Stmt.
///
seec::util::Maybe<std::unique_ptr<Explanation>, seec::Error>
ExplanationOfStmt::create(::clang::Stmt const *Node)
{
  char const *DescriptionKey = nullptr;
  seec::icu::FormatArgumentsWithNames DescriptionArguments;
  
  NodeLinks ExplanationLinks;
  
  // Find the appropriate description for the Stmt class.
  switch (Node->getStmtClass()) {
    case ::clang::Stmt::StmtClass::NoStmtClass:
      return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                  {"errors",
                                                   "CreateStmtNoStmtClass"}));
    
#define STMT(CLASS, PARENT)                           \
    case ::clang::Stmt::StmtClass::CLASS##Class:      \
      DescriptionKey = #CLASS;                        \
      addInfo(llvm::cast<::clang::CLASS const>(Node), \
              DescriptionArguments,                   \
              ExplanationLinks);                      \
      break;
#define ABSTRACT_STMT(STMT)
#include "clang/AST/StmtNodes.inc"
    
    default:
      return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                  {"errors",
                                                   "CreateStmtUnknownStmtClass"
                                                   }));
  }
  
  // Extract the raw description.
  UErrorCode Status = U_ZERO_ERROR;
  auto const Descriptions = seec::getResource("ClangEPV",
                                              Locale(),
                                              Status,
                                              "descriptions");
  auto const Description = Descriptions.getStringEx(DescriptionKey, Status);
  
  if (!U_SUCCESS(Status))
    return seec::Error(
              LazyMessageByRef::create("ClangEPV",
                                       {"errors", "DescriptionNotFound"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Format the raw description.
  UnicodeString FormattedDescription = seec::icu::format(Description,
                                                         DescriptionArguments,
                                                         Status);
  if (!U_SUCCESS(Status))
    return seec::Error(
              LazyMessageByRef::create("ClangEPV",
                                       {"errors", "DescriptionFormatFailed"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Get the indexed description.
  auto MaybeIndexed = seec::icu::IndexedString::from(FormattedDescription);
  if (!MaybeIndexed.assigned())
    return seec::Error(
              LazyMessageByRef::create("ClangEPV",
                                       {"errors", "DescriptionIndexFailed"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Produce the Explanation.
  auto Explained = new ExplanationOfStmt(Node,
                                         std::move(MaybeIndexed.get<0>()),
                                         std::move(ExplanationLinks));
  
  return std::unique_ptr<Explanation>(Explained);
}


} // namespace clang_epv (in seec)

} // namespace seec
