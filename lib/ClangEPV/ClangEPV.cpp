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
#include "seec/Preprocessor/Apply.h"

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

Formattable formatAsString(llvm::StringRef String) {
  return Formattable(UnicodeString::fromUTF8(String.str()));
}

Formattable formatAsString(::clang::BinaryOperatorKind Opcode) {
  switch (Opcode) {
#define SEEC_OPCODE_STRINGIZE(CODE) \
    case ::clang::BinaryOperatorKind::CODE: return #CODE;

SEEC_OPCODE_STRINGIZE(BO_PtrMemD)
SEEC_OPCODE_STRINGIZE(BO_PtrMemI)
SEEC_OPCODE_STRINGIZE(BO_Mul)
SEEC_OPCODE_STRINGIZE(BO_Div)
SEEC_OPCODE_STRINGIZE(BO_Rem)
SEEC_OPCODE_STRINGIZE(BO_Add)
SEEC_OPCODE_STRINGIZE(BO_Sub)
SEEC_OPCODE_STRINGIZE(BO_Shl)
SEEC_OPCODE_STRINGIZE(BO_Shr)
SEEC_OPCODE_STRINGIZE(BO_LT)
SEEC_OPCODE_STRINGIZE(BO_GT)
SEEC_OPCODE_STRINGIZE(BO_LE)
SEEC_OPCODE_STRINGIZE(BO_GE)
SEEC_OPCODE_STRINGIZE(BO_EQ)
SEEC_OPCODE_STRINGIZE(BO_NE)
SEEC_OPCODE_STRINGIZE(BO_And)
SEEC_OPCODE_STRINGIZE(BO_Xor)
SEEC_OPCODE_STRINGIZE(BO_Or)
SEEC_OPCODE_STRINGIZE(BO_LAnd)
SEEC_OPCODE_STRINGIZE(BO_LOr)
SEEC_OPCODE_STRINGIZE(BO_Assign)
SEEC_OPCODE_STRINGIZE(BO_MulAssign)
SEEC_OPCODE_STRINGIZE(BO_DivAssign)
SEEC_OPCODE_STRINGIZE(BO_RemAssign)
SEEC_OPCODE_STRINGIZE(BO_AddAssign)
SEEC_OPCODE_STRINGIZE(BO_SubAssign)
SEEC_OPCODE_STRINGIZE(BO_ShlAssign)
SEEC_OPCODE_STRINGIZE(BO_ShrAssign)
SEEC_OPCODE_STRINGIZE(BO_AndAssign)
SEEC_OPCODE_STRINGIZE(BO_XorAssign)
SEEC_OPCODE_STRINGIZE(BO_OrAssign)
SEEC_OPCODE_STRINGIZE(BO_Comma)

#undef SEEC_OPCODE_STRINGIZE
  }

  return formatAsString("<unknown opcode>");
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

// X-Macro generated specializations.
#define SEEC_STMT_LINK_ARG(NAME, TYPE, GETTER) \
  Arguments.add(NAME, formatAs##TYPE(Statement->GETTER));

#define SEEC_STMT_LINK_LINK(NAME, GETTER) \
  Links.add(NAME, Statement->GETTER);

#define SEEC_STMT_LINK(STMTCLASS, ARGUMENTS, LINKS)                            \
void addInfo(::clang::STMTCLASS const *Statement,                              \
             seec::icu::FormatArgumentsWithNames &Arguments,                   \
             NodeLinks &Links)                                                 \
{                                                                              \
  SEEC_PP_APPLY(SEEC_STMT_LINK_ARG, ARGUMENTS)                                 \
  SEEC_PP_APPLY(SEEC_STMT_LINK_LINK, LINKS)                                    \
}

#include "StmtLinks.def"

#undef SEEC_STMT_LINK_ARG
#undef SEEC_STMT_LINK_LINK

// Manual specializations.

/// \brief Specialization for DeclRefExpr
///
void addInfo(::clang::DeclRefExpr const *Statement,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  auto const Decl = Statement->getDecl();
  auto const Name = Statement->getNameInfo().getName();
  
  Arguments.add("name", formatAsString(Name.getAsString()));
  
  if (auto const D = llvm::dyn_cast<::clang::VarDecl>(Decl)) {
    Arguments.add("kind_general", formatAsString("Var"));
    Arguments.add("has_definition", formatAsBool(D->hasDefinition()));
  }
  else if (auto const D = llvm::dyn_cast<::clang::FunctionDecl>(Decl)) {
    Arguments.add("kind_general", formatAsString("Function"));
    Arguments.add("has_body", formatAsBool(D->hasBody()));
  }
  else if (auto const D = llvm::dyn_cast<::clang::EnumConstantDecl>(Decl)) {
    Arguments.add("kind_general", formatAsString("EnumConstant"));
    Arguments.add("init_val", formatAsString(D->getInitVal().toString(10)));
  }
  else {
    Arguments.add("kind_general", formatAsString("Other"));
  }
  
  Links.add("decl", Decl);
  Links.add("found_decl", Statement->getFoundDecl());
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
