//===- tools/seec-trace-view/ValueFormat.cpp ------------------------------===//
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

#include "seec/Clang/MappedProcessState.hpp"
#include "seec/Clang/MappedProcessTrace.hpp"
#include "seec/Clang/MappedValue.hpp"
#include "seec/ICU/Format.hpp"
#include "seec/ICU/Resources.hpp"

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

#include "ValueFormat.hpp"


static char const *getPointerDescriptionKey(seec::cm::ValueOfPointer const &P)
{
  if (P.isInMemory()) {
    if (!P.isCompletelyInitialized())
      return "PointerInMemoryUninitialized";
    if (P.getRawValue() == 0)
      return "PointerInMemoryNULL";
    if (P.isValidOpaque())
      return "PointerInMemoryOpaque";
    if (P.getDereferenceIndexLimit() == 0)
      return "PointerInMemoryInvalid";
    return "PointerInMemory";
  }
  else {
    if (P.getRawValue() == 0)
      return "PointerNULL";
    if (P.isValidOpaque())
      return "PointerOpaque";
    if (P.getDereferenceIndexLimit() == 0)
      return "PointerInvalid";
    return "Pointer";  
  }
}

UnicodeString getPrettyStringForInline(seec::cm::Value const &Value,
                                       seec::cm::ProcessState const &State,
                                       clang::Stmt const * const Stmt)
{
  auto const Kind = Value.getKind();
  
  if (Kind == seec::cm::Value::Kind::Pointer) {
    // Pointers are a special case because we don't want to display raw values
    // to the users (i.e. memory addresses).
    auto const &Pointer = llvm::cast<seec::cm::ValueOfPointer>(Value);
    
    if (Pointer.getCanonicalType()->isFunctionPointerType()
        && Pointer.getRawValue())
    {
      auto const &Trace = State.getProcessTrace();
      auto const MappedFn = Trace.getMappedFunctionAt(Pointer.getRawValue());
      if (MappedFn) {
        auto const ND = llvm::dyn_cast<clang::NamedDecl>(MappedFn->getDecl());
        if (ND) {
          return UnicodeString::fromUTF8(ND->getNameAsString());
        }
      }
    }

    UErrorCode Status = U_ZERO_ERROR;
    auto Resources = seec::getResource("SeeCClang",
                                       Locale::getDefault(),
                                       Status,
                                       "Values",
                                       "Descriptive");
    
    if (U_FAILURE(Status))
      return UnicodeString::fromUTF8("");
    
    auto const Key = getPointerDescriptionKey(Pointer);
    auto const String = Resources.getStringEx(Key, Status);
    
    if (U_FAILURE(Status))
      return UnicodeString::fromUTF8("");
    
    return String;
  }
  else {
    auto const Expr = llvm::dyn_cast<clang::Expr>(Stmt);

    if (Expr && Expr->isLValue()) {
      auto Result = seec::getString("SeeCClang",
                                    (char const * []){
                                      "Values", "Descriptive", "LValue"});
      if (Result.assigned<UnicodeString>())
        return Result.move<UnicodeString>();
      return UnicodeString::fromUTF8("");
    }
    else {
      return UnicodeString::fromUTF8(Value.getValueAsStringFull());
    }
  }
}

static UnicodeString &
truncateChar32(UnicodeString &Str, int32_t const CodePointLength)
{
  auto const CodeUnitLength = Str.moveIndex32(0, CodePointLength);
  Str.truncate(CodeUnitLength);
  return Str;
}

UnicodeString shortenValueString(UnicodeString ValueString, int32_t Length)
{
  // If the string is short enough already then return it as-is.
  if (!ValueString.hasMoreChar32Than(0, INT32_MAX, Length))
    return ValueString;
  
  UErrorCode Status = U_ZERO_ERROR;
  auto Resources = seec::getResource("SeeCClang",
                                     Locale::getDefault(),
                                     Status,
                                     "Values",
                                     "Descriptive");
  
  // If we can't get the localized shortening, then just truncate the string.
  if (U_FAILURE(Status))
    return truncateChar32(ValueString, Length);
  
  // Determine how many characters are required for the shortening.
  auto const Padding = seec::getIntEx(Resources, "ShortenedPadding", Status);
  if (U_FAILURE(Status))
    return truncateChar32(ValueString, Length);
  
  // If the padding would use too much room, use a placeholder instead.
  if (Padding >= Length) {
    auto Placeholder = Resources.getStringEx("TooShort", Status);
    if (U_SUCCESS(Status))
      return Placeholder;
    else
      return truncateChar32(ValueString, Length);
  }
  
  // Get the format string.
  auto const ShortenedFormat = Resources.getStringEx("Shortened", Status);
  if (U_FAILURE(Status))
    return truncateChar32(ValueString, Length);
  
  // Trim the string to make room for the shortening characters.
  truncateChar32(ValueString, Length - Padding);
  
  auto const Args = seec::icu::FormatArgumentsWithNames{}
                    .add("shortened", ValueString);
  
  auto const Shortened = seec::icu::format(ShortenedFormat, Args, Status);
  if (U_FAILURE(Status))
    return ValueString;
  
  return Shortened;
}
