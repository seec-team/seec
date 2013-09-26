//===- lib/RuntimeErrors/UnicodeFormatter.cpp -----------------------------===//
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

#include "seec/ICU/Format.hpp"
#include "seec/ICU/Output.hpp"
#include "seec/ICU/Resources.hpp"
#include "seec/RuntimeErrors/RuntimeErrors.hpp"
#include "seec/RuntimeErrors/UnicodeFormatter.hpp"
#include "seec/Util/Printing.hpp"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "unicode/fmtable.h"
#include "unicode/unistr.h"
#include "unicode/msgfmt.h"
#include "unicode/locid.h"

#include <string>

namespace seec {

namespace runtime_errors {


//===----------------------------------------------------------------------===//
// formatArg()
//===----------------------------------------------------------------------===//

Formattable formatArg(ArgAddress const &A) {
  std::string AddressString;

  llvm::raw_string_ostream AddressStringStream(AddressString);
  
  if (auto const Address = A.address())
    seec::util::write_hex_padded(AddressStringStream, A.address());
  else
    AddressStringStream << "NULL";
  
  AddressStringStream.flush();

  return Formattable(AddressString.c_str());
}

Formattable formatArg(ArgObject const &A) {
  return Formattable();
}

Formattable formatArg(ArgSelectBase const &A) {
  return Formattable(getCString(A.getSelectID(), A.getRawItemValue()));
}

Formattable formatArg(ArgSize const &A) {
  // TODO: convert size to string
  return Formattable(static_cast<int64_t>(A.size()));
}

Formattable formatArg(ArgOperand const &A) {
  return Formattable(static_cast<int64_t>(A.index()));
}

Formattable formatArg(ArgParameter const &A) {
  return Formattable(static_cast<int64_t>(A.index() + 1));
}

Formattable formatArg(ArgCharacter const &A) {
  char String[] = {A.character(), 0};
  return Formattable(String);
}

Formattable formatArg(Arg const &A) {
  switch (A.type()) {
    case ArgType::None:
      break;
#define SEEC_RUNERR_ARG(TYPE)                              \
    case ArgType::TYPE:                                    \
      return formatArg(static_cast<Arg##TYPE const &>(A));
#include "seec/RuntimeErrors/ArgumentTypes.def"
  }

  llvm_unreachable("Unknown type");
  return Formattable();
}


//===----------------------------------------------------------------------===//
// Description
//===----------------------------------------------------------------------===//

seec::Maybe<std::unique_ptr<Description>, seec::Error>
Description::create(RunError const &Error) {
  // Create descriptions for all of the additional errors.
  std::vector<std::unique_ptr<Description>> AdditionalDescriptions;
  
  for (auto const &Additional : Error.additional()) {
    auto AddDescription = Description::create(*Additional);
    
    if (AddDescription.assigned<seec::Error>()) {
      return seec::Maybe<std::unique_ptr<Description>, seec::Error>
                        (std::move(AddDescription.get<seec::Error>()));
    }
    
    assert(AddDescription.assigned(0));
    
    AdditionalDescriptions.emplace_back(std::move(AddDescription.get<0>()));
  }
  
  // Create the description for the parent error.
  auto const DescriptionKey = describe(Error.type());
  
  // Extract the raw description.
  UErrorCode Status = U_ZERO_ERROR;
  auto const Descriptions = seec::getResource("RuntimeErrors",
                                              Locale(),
                                              Status,
                                              "descriptions");
  auto const Message = Descriptions.getStringEx(DescriptionKey, Status);
  
  if (!U_SUCCESS(Status))
    return seec::Error(
              LazyMessageByRef::create("RuntimeErrors",
                                       {"errors", "DescriptionNotFound"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Format the arguments.
  seec::icu::FormatArgumentsWithNames DescriptionArguments;
  
  for (auto &Arg : Error.args()) {
    auto Name = getArgumentName(Error.type(), DescriptionArguments.size());
    DescriptionArguments.add(UnicodeString::fromUTF8(Name),
                             formatArg(*Arg));
  }
  
  // Format the raw description.
  UnicodeString FormattedDescription = seec::icu::format(Message,
                                                         DescriptionArguments,
                                                         Status);
  if (!U_SUCCESS(Status))
    return seec::Error(
              LazyMessageByRef::create("RuntimeErrors",
                                       {"errors", "DescriptionFormatFailed"},
                                       std::make_pair("key", DescriptionKey)));
  
  // Index the string.
  auto MaybeIndexed = seec::icu::IndexedString::from(FormattedDescription);
  if (!MaybeIndexed.assigned())
    return seec::Error(
              LazyMessageByRef::create("RuntimeErrors",
                                       {"errors", "DescriptionIndexFailed"},
                                       std::make_pair("key", DescriptionKey)));
  
  return std::unique_ptr<Description>
                        (new Description(std::move(MaybeIndexed.get<0>()),
                                         std::move(AdditionalDescriptions)));
}


//===----------------------------------------------------------------------===//
// DescriptionPrinterUnicode
//===----------------------------------------------------------------------===//

void DescriptionPrinterUnicode::appendDescription(Description const &Desc,
                                                  unsigned const Indent)
{
  if (CombinedString.length())
    CombinedString.append(Separator);
  
  for (unsigned i = 0; i < Indent; ++i)
    CombinedString.append(Indentation);
  
  CombinedString.append(Desc.getString());
  
  for (auto const &Additional : Desc.getAdditional())
    appendDescription(*Additional, Indent + 1);
}

DescriptionPrinterUnicode::
DescriptionPrinterUnicode(std::unique_ptr<Description> WithDescription,
                          UnicodeString WithSeparator,
                          UnicodeString WithIndentation)
: TheDescription(std::move(WithDescription)),
  Separator(std::move(WithSeparator)),
  Indentation(std::move(WithIndentation)),
  CombinedString()
{
  assert(TheDescription && "Description is null!");
  
  appendDescription(*TheDescription, 0);
}


} // namespace runtime_errors (in seec)

} // namespace seec
