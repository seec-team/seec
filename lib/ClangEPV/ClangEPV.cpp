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
#include "seec/Util/FixedWidthIntTypes.hpp"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclFriend.h"
#include "clang/AST/DeclOpenMP.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/StmtObjC.h"
#include "clang/AST/StmtOpenMP.h"

#include "llvm/Support/raw_ostream.h"

#include "unicode/fmtable.h"
#include "unicode/msgfmt.h"

#include <vector>


namespace seec {

namespace clang_epv {


//===----------------------------------------------------------------------===//
// Formattable helpers.
//===----------------------------------------------------------------------===//

Formattable formatAsBool(bool Value) {
  return Value ? Formattable("true") : Formattable("false");
}

Formattable formatAsInt(int32_t Value) { return Formattable(Value); }

Formattable formatAsInt(int64_t Value) { return Formattable(Value); }

Formattable formatAsInt(unsigned Value) {
  return formatAsInt(int64_t(Value));
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

  llvm_unreachable("unknown BinaryOperatorKind");
  return formatAsString("<unknown BinaryOperatorKind>");
}

Formattable formatAsString(::clang::UnaryOperatorKind Opcode) {
  switch (Opcode) {
#define SEEC_OPCODE_STRINGIZE(CODE) \
    case ::clang::UnaryOperatorKind::CODE: return #CODE;

SEEC_OPCODE_STRINGIZE(UO_PostInc)
SEEC_OPCODE_STRINGIZE(UO_PostDec)
SEEC_OPCODE_STRINGIZE(UO_PreInc)
SEEC_OPCODE_STRINGIZE(UO_PreDec)
SEEC_OPCODE_STRINGIZE(UO_AddrOf)
SEEC_OPCODE_STRINGIZE(UO_Deref)
SEEC_OPCODE_STRINGIZE(UO_Plus)
SEEC_OPCODE_STRINGIZE(UO_Minus)
SEEC_OPCODE_STRINGIZE(UO_Not)
SEEC_OPCODE_STRINGIZE(UO_LNot)
SEEC_OPCODE_STRINGIZE(UO_Real)
SEEC_OPCODE_STRINGIZE(UO_Imag)
SEEC_OPCODE_STRINGIZE(UO_Extension)

#undef SEEC_OPCODE_STRINGIZE
  }
  
  llvm_unreachable("unknown UnaryOperatorKind");
  return formatAsString("<unknown UnaryOperatorKind>");
}

Formattable formatAsString(::clang::UnaryExprOrTypeTrait Kind) {
  switch (Kind) {
#define SEEC_KIND_STRINGIZE(MEMBER) \
    case ::clang::UnaryExprOrTypeTrait::MEMBER: return #MEMBER;

SEEC_KIND_STRINGIZE(UETT_SizeOf)
SEEC_KIND_STRINGIZE(UETT_AlignOf)
SEEC_KIND_STRINGIZE(UETT_VecStep)

#undef SEEC_KIND_STRINGIZE
  }
  
  llvm_unreachable("unknown UnaryExprOrTypeTrait");
  return formatAsString("<unknown UnaryExprOrTypeTrait>");
}

Formattable formatAsString(::clang::Type const *T) {
  return formatAsString(::clang::QualType::getAsString(T, clang::Qualifiers()));
}

Formattable formatAsString(::clang::QualType QT) {
  return formatAsString(QT.getAsString());
}

Formattable formatAsString(::clang::CharacterLiteral::CharacterKind Kind) {
  switch (Kind) {
    case ::clang::CharacterLiteral::CharacterKind::Ascii:
      return formatAsString("ASCII");
    case ::clang::CharacterLiteral::CharacterKind::Wide:
      return formatAsString("Wide");
    case ::clang::CharacterLiteral::CharacterKind::UTF16:
      return formatAsString("UTF-16");
    case ::clang::CharacterLiteral::CharacterKind::UTF32:
      return formatAsString("UTF-32");
  }
  
  llvm_unreachable("unknown CharacterKind");
  return formatAsString("<unknown CharacterKind>");
}

Formattable formatAsString(::clang::StringLiteral::StringKind Kind) {
  switch (Kind) {
    case ::clang::StringLiteral::StringKind::Ascii:
      return formatAsString("ASCII");
    case ::clang::StringLiteral::StringKind::Wide:
      return formatAsString("Wide");
    case ::clang::StringLiteral::StringKind::UTF8:
      return formatAsString("UTF-8");
    case ::clang::StringLiteral::StringKind::UTF16:
      return formatAsString("UTF-16");
    case ::clang::StringLiteral::StringKind::UTF32:
      return formatAsString("UTF-32");
  }
  
  llvm_unreachable("unknown StringKind");
  return formatAsString("<unknown StringKind>");
}

Formattable formatAsGeneralTypeString(::clang::Type const *T) {
  if (!T)
    return formatAsString("<null>");
  
  if (T->isIntegerType())
    return formatAsString("Integer");
  if (T->isEnumeralType())
    return formatAsString("Enumeral");
  if (T->isBooleanType())
    return formatAsString("Boolean");
  if (T->isAnyCharacterType())
    return formatAsString("Char");
  if (T->isRealFloatingType())
    return formatAsString("Floating");
  if (T->isVoidType())
    return formatAsString("Void");
  if (T->isPointerType())
    return formatAsString("Pointer");
  
  return formatAsString("<unknown type>");
}


//===----------------------------------------------------------------------===//
// Explanation
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// explain()
//===----------------------------------------------------------------------===//

seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Decl const *Node)
{
  if (Node == nullptr)
    return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                {"errors",
                                                 "ExplainNullDecl"}));
  
  return ExplanationOfDecl::create(Node, nullptr);
}

seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Decl const *Node,
        RuntimeValueLookup const &ValueLookup)
{
  if (Node == nullptr)
    return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                {"errors",
                                                 "ExplainNullDecl"}));
  
  return ExplanationOfDecl::create(Node, &ValueLookup);
}

seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Stmt const *Node)
{
  if (Node == nullptr)
    return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                {"errors",
                                                 "ExplainNullStmt"}));
  
  return ExplanationOfStmt::create(Node, nullptr);
}

seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
explain(::clang::Stmt const *Node,
        RuntimeValueLookup const &ValueLookup)
{
  if (Node == nullptr)
    return seec::Error(LazyMessageByRef::create("ClangEPV",
                                                {"errors",
                                                 "ExplainNullStmt"}));
  
  return ExplanationOfStmt::create(Node, &ValueLookup);
}


//===----------------------------------------------------------------------===//
// Explanation generation helpers.
//===----------------------------------------------------------------------===//

void addRuntimeValue(::clang::Stmt const *ForStatement,
                     char const *Name,
                     RuntimeValueLookup const &ValueLookup,
                     seec::icu::FormatArgumentsWithNames &Arguments)
{
  UnicodeString UnicodeName(Name);
  UnicodeString UnicodeHasName = UnicodeString("has_") + UnicodeName;
  UnicodeString UnicodeHasBoolName = UnicodeString("has_bool_") + UnicodeName;
  
  if (ForStatement && ValueLookup.isValueAvailableFor(ForStatement)) {
    auto const ValueString = ValueLookup.getValueString(ForStatement);
    if (!ValueString.empty()) {
      // Add the runtime value.
      Arguments.add(UnicodeHasName, formatAsBool(true));
      Arguments.add(UnicodeName, formatAsString(ValueString));
    }
    
    auto const ValueAsBool = ValueLookup.getValueAsBool(ForStatement);
    if (ValueAsBool.assigned()) {
      UnicodeString UnicodeBoolName = UnicodeString("bool_") + UnicodeName;
      Arguments.add(UnicodeHasBoolName, formatAsBool(true));
      Arguments.add(UnicodeBoolName, formatAsBool(ValueAsBool.get<bool>()));
    }
  }
  
  // No runtime value found.
  Arguments.add(UnicodeHasName, formatAsBool(false));
  Arguments.add(UnicodeHasBoolName, formatAsBool(false));
}


//===----------------------------------------------------------------------===//
// addInfo() for ::clang::Decl
//===----------------------------------------------------------------------===//

/// \brief Catch all non-specialized Decl cases.
///
void addInfo(::clang::Decl const *Decl,
             RuntimeValueLookup const *ValueLookup,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{}

// X-Macro generated specializations.
#define SEEC_DECL_LINK_ARG(NAME, TYPE, GETTER)                                 \
  Arguments.add(NAME, formatAs##TYPE(Declaration->GETTER));

#define SEEC_DECL_LINK_RTV(NAME, GETTER)                                       \
  addRuntimeValue(Declaration->GETTER, NAME, *ValueLookup, Arguments);

#define SEEC_DECL_LINK_LINK(NAME, GETTER)                                      \
  Links.add(NAME, Declaration->GETTER);

#define SEEC_DECL_LINK(DECLCLASS, ARGUMENTS, RTVALUES, LINKS)                  \
void addInfo(::clang::DECLCLASS const *Declaration,                            \
             RuntimeValueLookup const *ValueLookup,                            \
             seec::icu::FormatArgumentsWithNames &Arguments,                   \
             NodeLinks &Links)                                                 \
{                                                                              \
  SEEC_PP_APPLY(SEEC_DECL_LINK_ARG, ARGUMENTS)                                 \
  if (ValueLookup) {                                                           \
    SEEC_PP_APPLY(SEEC_DECL_LINK_RTV, RTVALUES)                                \
  }                                                                            \
  SEEC_PP_APPLY(SEEC_DECL_LINK_LINK, LINKS)                                    \
}

#include "DeclLinks.def"

#undef SEEC_DECL_LINK_ARG
#undef SEEC_DECL_LINK_RTV
#undef SEEC_DECL_LINK_LINK


//===----------------------------------------------------------------------===//
// addInfoForDerivedAndBase() for ::clang::Decl
//===----------------------------------------------------------------------===//

void addInfoForDerivedAndBase(::clang::Decl const *Node,
                              RuntimeValueLookup const *ValueLookup,
                              seec::icu::FormatArgumentsWithNames &Arguments,
                              NodeLinks &Links)
{
  addInfo(Node, ValueLookup, Arguments, Links);
}

#define DECL(DERIVED, BASE)                                                    \
void addInfoForDerivedAndBase(::clang::DERIVED##Decl const *Node,              \
                              RuntimeValueLookup const *ValueLookup,           \
                              seec::icu::FormatArgumentsWithNames &Arguments,  \
                              NodeLinks &Links) {                              \
  auto const BasePtr = static_cast< ::clang::BASE const *>(Node);              \
  addInfoForDerivedAndBase(BasePtr, ValueLookup, Arguments, Links);            \
  addInfo(Node, ValueLookup, Arguments, Links);                                \
}
#include "clang/AST/DeclNodes.inc"


//===----------------------------------------------------------------------===//
// ExplanationOfDecl
//===----------------------------------------------------------------------===//

seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
ExplanationOfDecl::create(::clang::Decl const *Node,
                          RuntimeValueLookup const *ValueLookup)
{
  char const *DescriptionKey = nullptr;
  seec::icu::FormatArgumentsWithNames DescriptionArguments;
  
  NodeLinks ExplanationLinks;
  
  // Find the appropriate description for the Decl kind.
  switch (Node->getKind()) {
#define DECL(DERIVED, BASE)                                                \
    case ::clang::Decl::Kind::DERIVED:                                     \
      DescriptionKey = #DERIVED;                                           \
      addInfoForDerivedAndBase(llvm::cast< ::clang::DERIVED##Decl >(Node), \
                               ValueLookup,                                \
                               DescriptionArguments,                       \
                               ExplanationLinks);                          \
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
             RuntimeValueLookup const *ValueLookup,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{}

// X-Macro generated specializations.
#define SEEC_STMT_LINK_ARG(NAME, TYPE, GETTER) \
  Arguments.add(NAME, formatAs##TYPE(Statement->GETTER));

#define SEEC_STMT_LINK_RTV(NAME, GETTER)                                       \
  addRuntimeValue(Statement->GETTER, NAME, *ValueLookup, Arguments);

#define SEEC_STMT_LINK_LINK(NAME, GETTER) \
  Links.add(NAME, Statement->GETTER);

#define SEEC_STMT_LINK(STMTCLASS, ARGUMENTS, RTVALUES, LINKS)                  \
void addInfo(::clang::STMTCLASS const *Statement,                              \
             RuntimeValueLookup const *ValueLookup,                            \
             seec::icu::FormatArgumentsWithNames &Arguments,                   \
             NodeLinks &Links)                                                 \
{                                                                              \
  SEEC_PP_APPLY(SEEC_STMT_LINK_ARG, ARGUMENTS)                                 \
  if (ValueLookup) {                                                           \
    addRuntimeValue(Statement, "rtv_of_this", *ValueLookup, Arguments);        \
    SEEC_PP_APPLY(SEEC_STMT_LINK_RTV, RTVALUES)                                \
  }                                                                            \
  SEEC_PP_APPLY(SEEC_STMT_LINK_LINK, LINKS)                                    \
}

#include "StmtLinks.def"

#undef SEEC_STMT_LINK_ARG
#undef SEEC_STMT_LINK_RTV
#undef SEEC_STMT_LINK_LINK

// Manual specializations.

/// \brief Specialization for ArraySubscriptExpr
///
void addInfo(::clang::ArraySubscriptExpr const *Statement,
             RuntimeValueLookup const *ValueLookup,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  auto const Base = Statement->getBase();
  auto const Idx = Statement->getIdx();
  
  // Arguments.
  Arguments.add("is_lhs_base", formatAsBool(Statement->getLHS() == Base));
  Arguments.add("base_type_general",
                formatAsGeneralTypeString(Base->getType().getTypePtr()));
  Arguments.add("idx_type_general",
                formatAsGeneralTypeString(Idx->getType().getTypePtr()));
  
  // Runtime values.
  if (ValueLookup) {
    addRuntimeValue(Statement, "rtv_of_this", *ValueLookup, Arguments);
    addRuntimeValue(Base, "rtv_of_base", *ValueLookup, Arguments);
    addRuntimeValue(Idx, "rtv_of_idx", *ValueLookup, Arguments);
  }
  
  // Links.
  Links.add("base", Base);
  Links.add("idx", Idx);
}

/// \brief Specialization for CallExpr
///
void addInfo(::clang::CallExpr const *Statement,
             RuntimeValueLookup const *ValueLookup,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  auto const Callee = Statement->getCallee();
  auto const CalleeDecl = Statement->getCalleeDecl();
  auto const DirectCallee = Statement->getDirectCallee();
  
  // Arguments.
  Arguments.add("has_callee", formatAsBool(Callee));
  Arguments.add("has_callee_decl", formatAsBool(CalleeDecl));
  Arguments.add("has_direct_callee", formatAsBool(DirectCallee));
  Arguments.add("num_args", formatAsInt(int64_t(Statement->getNumArgs())));
  Arguments.add("general_type",
                formatAsGeneralTypeString(Statement->getType().getTypePtr()));
  
  // Runtime values.
  if (ValueLookup) {
    addRuntimeValue(Statement, "rtv_of_this", *ValueLookup, Arguments);
  }
  
  // Links.
  Links.add("callee_expr", Callee);
  Links.add("direct_callee_decl", DirectCallee);
}

/// \brief Specialization for DeclRefExpr
///
void addInfo(::clang::DeclRefExpr const *Statement,
             RuntimeValueLookup const *ValueLookup,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  auto const Decl = Statement->getDecl();
  auto const Name = Statement->getNameInfo().getName();
  
  // Arguments.
  Arguments.add("name", formatAsString(Name.getAsString()));
  
  if (auto const D = llvm::dyn_cast< ::clang::VarDecl>(Decl)) {
    Arguments.add("kind_general", formatAsString("Var"));
    Arguments.add("has_definition", formatAsBool(D->hasDefinition()));
  }
  else if (auto const D = llvm::dyn_cast< ::clang::FunctionDecl>(Decl)) {
    Arguments.add("kind_general", formatAsString("Function"));
    Arguments.add("has_body", formatAsBool(D->hasBody()));
  }
  else if (auto const D = llvm::dyn_cast< ::clang::EnumConstantDecl>(Decl)) {
    Arguments.add("kind_general", formatAsString("EnumConstant"));
    Arguments.add("init_val", formatAsString(D->getInitVal().toString(10)));
  }
  else {
    Arguments.add("kind_general", formatAsString("Other"));
  }
  
  // Runtime values.
  if (ValueLookup) {
    addRuntimeValue(Statement, "rtv_of_this", *ValueLookup, Arguments);
  }
  
  // Links.
  Links.add("decl", Decl);
  Links.add("found_decl", Statement->getFoundDecl());
}

/// \brief Specialization for DeclStmt
///
void addInfo(::clang::DeclStmt const *Statement,
             RuntimeValueLookup const *ValueLookup,
             seec::icu::FormatArgumentsWithNames &Arguments,
             NodeLinks &Links)
{
  Arguments.add("is_single_decl", formatAsBool(Statement->isSingleDecl()));
  
  if (Statement->isSingleDecl()) {
    Links.add("single_decl", Statement->getSingleDecl());
  }
  else {
    // TODO.
  }
}


/// \brief Attempt to create an Explanation for a ::clang::Stmt.
///
seec::Maybe<std::unique_ptr<Explanation>, seec::Error>
ExplanationOfStmt::create(::clang::Stmt const *Node,
                          RuntimeValueLookup const *ValueLookup)
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
    
#define STMT(CLASS, PARENT)                            \
    case ::clang::Stmt::StmtClass::CLASS##Class:       \
      DescriptionKey = #CLASS;                         \
      addInfo(llvm::cast< ::clang::CLASS const>(Node), \
              ValueLookup,                             \
              DescriptionArguments,                    \
              ExplanationLinks);                       \
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
