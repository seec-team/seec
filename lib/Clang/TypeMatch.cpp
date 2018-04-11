//===- lib/Clang/TypeMatch.cpp --------------------------------------------===//
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

#include "seec/Clang/TypeMatch.hpp"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Type.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>

namespace seec {

namespace cm {

using HistoryTy = std::vector<clang::Type const *>;

bool matchImpl(::clang::ASTContext const &AContext,
               HistoryTy &AHistory,
               ::clang::Type const *AType,
               ::clang::ASTContext const &BContext,
               HistoryTy &BHistory,
               ::clang::Type const *BType);

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::BuiltinType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::BuiltinType const *B)
{
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  return A->getKind() == B->getKind();
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::ComplexType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::ComplexType const *B)
{
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getElementType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getElementType().getTypePtr());
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::PointerType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::PointerType const *B)
{
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getPointeeType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getPointeeType().getTypePtr());
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::BlockPointerType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::BlockPointerType const *B)
{
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getPointeeType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getPointeeType().getTypePtr());
}

// This will handle \c LValueReferenceType and \c RValueReferenceType. Note: A
// and B will be the same derived type, as that is checked in \c matchImpl().
//
static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::ReferenceType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::ReferenceType const *B)
{
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getPointeeType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getPointeeType().getTypePtr());
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::MemberPointerType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::MemberPointerType const *B)
{
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getPointeeType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getPointeeType().getTypePtr());
}

// This handles ConstantArrayType, IncompleteArrayType, VariableArrayType.
//
static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::ArrayType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::ArrayType const *B)
{
  return matchImpl(AContext,
                   AHistory,
                   A->getElementType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getElementType().getTypePtr());
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::VectorType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::VectorType const *B)
{
  assert(A && B);
  
  if (A->getNumElements() != B->getNumElements())
    return false;
  
  if (A->getVectorKind() != B->getVectorKind())
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getElementType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getElementType().getTypePtr());
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::FunctionProtoType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::FunctionProtoType const *B)
{
  // TODO.
  
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::FunctionNoProtoType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::FunctionNoProtoType const *B)
{
  if ((A->getCallConv() != B->getCallConv())
      || (A->isConst() != B->isConst()))
    return false;
  
  return matchImpl(AContext,
                   AHistory,
                   A->getReturnType().getTypePtr(),
                   BContext,
                   BHistory,
                   B->getReturnType().getTypePtr());
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::RecordType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::RecordType const *B)
{
  auto const ADecl = A->getDecl();
  auto const BDecl = B->getDecl();
  
  if (ADecl->getTagKind() != BDecl->getTagKind())
    return false;
  
  auto const ADef = ADecl->getDefinition();
  auto const BDef = BDecl->getDefinition();
  
  if (!ADef || !BDef)
    return false; // TODO: Can we decide if this is a match in some cases?
  
  // Check the size and alignment of each type.
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  // Check that the fields in each definition are matching.
  auto const AEnd = ADef->field_end();
  auto const BEnd = BDef->field_end();
  
  for (auto AIt = ADef->field_begin(), BIt = BDef->field_begin(); ; ++AIt,++BIt)
  {
    if (AIt == AEnd && BIt == BEnd)
      break;
    
    if (AIt == AEnd || BIt == BEnd)
      return false;
    
    auto const AFieldType = AIt->getType().getTypePtr();
    auto const BFieldType = BIt->getType().getTypePtr();
    
    if (!matchImpl(AContext,AHistory,AFieldType,BContext,BHistory,BFieldType))
      return false;
  }
  
  return true;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::EnumType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::EnumType const *B)
{
  auto const ADecl = A->getDecl();
  auto const BDecl = B->getDecl();
  
  if (ADecl->getTagKind() != BDecl->getTagKind())
    return false;
  
  auto const ADef = ADecl->getDefinition();
  auto const BDef = BDecl->getDefinition();
  
  if (!ADef || !BDef)
    return false; // TODO: Can we decide if this is a match in some cases?
  
  // Check the size and alignment of each type.
  auto const ATypeInfo = AContext.getTypeInfo(A);
  auto const BTypeInfo = BContext.getTypeInfo(B);
  if (ATypeInfo.Width != BTypeInfo.Width || ATypeInfo.Align != BTypeInfo.Align)
    return false;
  
  // Check that the underlying type of the enums matches.
  auto const AEnumType = ADef->getIntegerType().getTypePtr();
  auto const BEnumType = BDef->getIntegerType().getTypePtr();
    
  if (!matchImpl(AContext,AHistory,AEnumType,BContext,BHistory,BEnumType))
    return false;
  
  // Check that the enums have the same values.
  auto const AEnd = ADef->enumerator_end();
  auto const BEnd = BDef->enumerator_end();
  
  for (auto AIt = ADef->enumerator_begin(), BIt = BDef->enumerator_begin();
       ; ++AIt, ++BIt)
  {
    if (AIt == AEnd && BIt == BEnd)
      break;
    
    if (AIt == AEnd || BIt == BEnd)
      return false;
    
    if (AIt->getInitVal() != BIt->getInitVal())
      return false;
  }
  
  return true;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::AutoType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::AutoType const *B)
{
  llvm_unreachable("matchType: AutoType not supported.");
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::DeducedTemplateSpecializationType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::DeducedTemplateSpecializationType const *B)
{
  llvm_unreachable(
    "matchType: DeducedTemplateSpecializationType not supported.");
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::ObjCObjectType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::ObjCObjectType const *B)
{
  llvm_unreachable("matchType: ObjCObjectType not supported.");
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::ObjCInterfaceType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::ObjCInterfaceType const *B)
{
  llvm_unreachable("matchType: ObjCInterfaceType not supported.");
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::ObjCObjectPointerType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::ObjCObjectPointerType const *B)
{
  llvm_unreachable("matchType: ObjCObjectPointerType not supported.");
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::PipeType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::PipeType const *B)
{
  llvm_unreachable("matchType: PipeType not supported.");
  return false;
}

static bool matchType(::clang::ASTContext const &AContext,
                      HistoryTy &AHistory,
                      ::clang::AtomicType const *A,
                      ::clang::ASTContext const &BContext,
                      HistoryTy &BHistory,
                      ::clang::AtomicType const *B)
{
  llvm_unreachable("matchType: AtomicType not supported.");
  return false;
}

bool matchImpl(::clang::ASTContext const &AContext,
               HistoryTy &AHistory,
               ::clang::Type const *AType,
               ::clang::ASTContext const &BContext,
               HistoryTy &BHistory,
               ::clang::Type const *BType)
{
  // Ensure that the types are non-null.
  if (!AType || !BType)
    return false;

  auto const ACanon = AType->getCanonicalTypeInternal().getTypePtr();
  auto const BCanon = BType->getCanonicalTypeInternal().getTypePtr();

  if (ACanon->getTypeClass() != BCanon->getTypeClass())
    return false;

  auto AHistIt = find(AHistory.begin(), AHistory.end(), AType);
  auto BHistIt = find(BHistory.begin(), BHistory.end(), BType);

  if (distance(AHistory.begin(), AHistIt)
      != distance(BHistory.begin(), BHistIt))
    return false;
  else if (AHistIt != AHistory.end())
    return true;

  AHistory.push_back(AType);
  BHistory.push_back(BType);

  switch (ACanon->getTypeClass()) {
#define ABSTRACT_TYPE(CLASS, BASE)
#define NON_CANONICAL_TYPE(CLASS, BASE) // Ignore non canonical types.
#define DEPENDENT_TYPE(CLASS, BASE) // Don't support uninstantiated templates.
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(CLASS, BASE)
#define TYPE(CLASS, BASE)                                                      \
    case ::clang::Type::CLASS:                                                 \
      return matchType(AContext,                                               \
                       AHistory,                                               \
                       llvm::dyn_cast< ::clang::CLASS ## Type >(ACanon),       \
                       BContext,                                               \
                       BHistory,                                               \
                       llvm::dyn_cast< ::clang::CLASS ## Type >(BCanon));
#include "clang/AST/TypeNodes.def"
    default:
      break;
  }

  llvm_unreachable("type class not handled in switch.");
  return false;
}

bool matchImpl(::clang::ASTContext const &AContext,
               ::clang::Type const *AType,
               ::clang::ASTContext const &BContext,
               ::clang::Type const *BType)
{
  HistoryTy AHistory, BHistory;

  return matchImpl(AContext, AHistory, AType, BContext, BHistory, BType);
}

} // namespace cm

} // namespace seec
